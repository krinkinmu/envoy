// I think I need to explain how the threading model implemented
// here came to be, why it's bad and what should a better model
// do. Partially because it explains quite a few complexities in
// this code and partially because it reveals something about how
// Envoy works.
//
// Envoy creates a fixed number of worker threads during startup.
// Those threads are special in multiple ways:
//
// 1. They are setup in Envoy specific way that allows them to
//    call Envoy functions - a thread that wasn't setup in such
//    a way cannot, in general, call Envoy functions;
// 2. Connections that Envoy receives once assigend to a thread,
//    as far as I understand, stay with the thread - so a lot of
//    processing related to the same connection in Envoy stays in
//    the same thread - on the flip side, many data structures in
//    Envoy are not protected to be accessed concurrently from
//    multiple threads;
// 3. The same thread can handle multiple connections at the same
//    time switching between them, one processing on one of the
//    connections cannot make further progress and has to be
//    blocked.
//
// This is actually a rather typical setup for async IO system
// relying on non-blocking sockets and polling mechanisms like
// select/poll/epoll and similar.
//
// Now, let's talk a little bit about threading in Hyperlight.
// When you create a Hyperlight sandbox Hyperlight underneath
// creates a new thread that manages communications with the
// hypervisor (e.g. KVM or Hyper-V). A typical interaction would
// work like that:
//
// 1. Thread A creates a hyperlight sandbox, which in turn
//    creates thread B that manages hypervisor;
// 2. Thread A, after creating sandbox, calls sandbox to execute
//    a function - at this point Thread A is blocked until the
//    call completes;
// 3. If sandbox need to make calls into the host (e.g. to execute
//    write function below), the call will be made from Thread B.
//
// The Envoy threading model and Hyperlight threading model don't
// work together without intermediary:
//
// 1. If we call into hyperlight sandbox from Envoy thread (in the
//    example above, Thread A is Envoy thread) this call will block
//    and Thread A will not be able to do anything until the call
//    finishes - that's not optimal, but at least not incorrect;
// 2. Thread B in the example above is created by hyperlight and
//    therefore it's not an Envoy thread, so Thread B cannot call
//    into any Envoy functions because it's only allowed for Envoy
//    threads - this is pretty bad because it severly limits what
//    kind of callbacks can hyperlight sandbox make.
//
// Now, the current solution sort-of deals with the problem 2, but
// does not deal with the problem 1. The way the current solution
// works is as follows:
//
// 1. First of all, we have 3 threads that are involved:
//
//    1. Thread A - Envoy thread, HyperlightFilter::onData or
//       HyperlightFilter::onNewConnection methods are called from
//       this thread;
//    2. Thread B - guest dispatcher thread, is a thread executing
//       HyperlightFilter::guestCallDispatchLoop function, this is
//       not an Envoy thread and it cannot call Envoy functions;
//    3. Thread C - hyperlight thread, this is also not an Envoy
//       thread and it cannot call Envoy functions;
//
// 2. We want to avoid blocking Thread A (Envoy thread) on hyperlight
//    calls, because it's the only thread we have that can call Envoy
//    functions, so we need to save it for that. Therefore, this
//    thread never calls hyperlight directly and instead sends a
//    message to the Thread B (guest dispatcher thread) to call
//    hyperlight by adding a task to the guest_calls_ queue instead.
//
// 3. When Thread C (hyperlight thread) needs to call into host and
//    execute an Envoy function, it cannot do that directly.
//    Therefore instead of directly calling an Envoy function it adds
//    a task to the host_calls_ queue instead.
//
// 4. The guest_call_ queue is processed by Thread B (guest dispatcher
//    thread) and host_calls_ queue is processed by Thread A (Envoy
//    thread), because only Envoy thread can run functions in the
//    host_calls_ queue.
//
// So the control flow might go like this:
//
// 1. [In Envoy Thread] onData is called:
//
//    1. [In Envoy Thread] a task to call WASM added to the guest_calls_
//    2. [In Envoy Thread] start waiting for the WASM call to complete or
//       for a guest call to arrive
//
// 2. [In Guest Dispatcher Thread] we notice a new task in the
//    guest_calls_
//
//    1. [In Guest Dispatcher Thread] we call hyperlight
//    2. [In Guest Dispatcher Thread] we block until hyperlight call
//       completes
//
// 3. [In Hyperlight Thread] we start running WASM module
//
//    1. [In Hyperlight Thread] WASM module calls `write` host function
//    2. [In Hyperlight Thread] We add a task to the host_calls_
//    3. [In Hyperlight Thread] we block until the host call completes
//
// 4. [In Envoy Thread] we notice a new task in host_calls_
//
//    1. [In Envoy Thread] we execute the task
//    2. [In Envoy Thread] we mark host call as complete
//    3. [In Envoy Thread] start waiting for the WASM call to complete
//       or for a guest call to arrive
//
// 5. [In Hyperlight Thread] we notice that host call completed
//
//    1. [In Hyperlight Thread] we continue execution of WASM module
//    2. [In Hyperlight Thread] WASM module completes the execution
//       and exits
//    3. [In Hyperlight Thread] we mark hyperlight call as complete
//
// 6. [In Guest Dispatcher Thread] we notice that hyperlight call
//    completed
//
//    1. [In Guest Dispatcher Thread] we mark guest call as complete
//
// 7. [In Envoy Thread] we notice that guest call completed
//
//    1. [In Envoy Thread] we finish onData callback and return.
//
// While this threading model works, it has some downsides:
//
// 1. We probably shouldn't create 2 new threads for each connection
//    - ideally we should either use a thread pool or avoid creating
//    new threads all togeher;
// 2. In this model, Envoy thread remains blocked while it's running
//    onData function and it will often sleep doing nothing just
//    waiting for things to happen - we should allow the Envoy thread
//    to process other connections while we are waiting.
//
// NOTE: Specifically with regard to the downside 2, while Envoy
// thread sleeps either guest dispatcher thread or the hyperlight
// thread do work, so it's not really that different from Envoy thread
// just doing all those computations by itself.
//
// However, even if we are not wasting resources, one heavy computation
// in an Envoy thread will affect latency of all connection associated
// with the same thread. So in the ideal world, we should not monopolize
// Envoy thread shared by multiple connections and release it
// periodically to process other connections.
#include "source/extensions/filters/network/hyperlight/hyperlight.h"

#include "envoy/buffer/buffer.h"
#include "envoy/common/exception.h"
#include "envoy/network/connection.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"
#include "source/extensions/common/hyperlight/hyperlight.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace Hyperlight {

HyperlightFilter::HyperlightFilter() : guest_dispatcher_([this]() { guestCallDispatchLoop(); }) {}

HyperlightFilter::~HyperlightFilter() {
  {
    std::unique_lock<std::mutex> lock(mux_);
    guest_done_ = true;
  }
  cond_.notify_all();
  guest_dispatcher_.join();
}

void HyperlightFilter::guestCallDispatchLoop() {
  while (true) {
    {
      std::unique_lock<std::mutex> lock(mux_);
      // We are waiting for at least one of the following to
      // becore true:
      //
      // 1. The main Envoy thread wants to shut us down - it
      //    will be signalled by setting guest_done_ = true;
      // 2. This thread has some work to do - the tasks this
      //    thread needs to do will be put in the guest_call_
      //    queue, so we are checking if the queue has
      //    something.
      cond_.wait(lock, [this]() -> bool { return guest_done_ || !guest_calls_.empty(); });
    }

    bool had_work = false;
    bool exit = false;

    {
      std::unique_lock<std::mutex> lock(mux_);
      had_work = !guest_calls_.empty();
      while (!guest_calls_.empty()) {
        auto call = guest_calls_.front();
        lock.unlock();

        // We can't hold the mutex while we are calling into
        // hyperlight. The WASM module that hyperlight
        // executes may call back into Envoy and if we hold
        // a mutex here it will result in a deadlock.
        call();

        lock.lock();
        guest_calls_.pop();
      }
      exit = guest_done_;
    }

    if (had_work) {
      cond_.notify_all();
    }
    if (exit) {
      return;
    }
  }
}

Network::FilterStatus HyperlightFilter::onNewConnection() {
  return Network::FilterStatus::Continue;
}

Network::FilterStatus HyperlightFilter::onData(Buffer::Instance& buf, bool) {
  ENVOY_LOG(info, "hyperlight filter received {} bytes", buf.length());
  if (sandbox_) {
    // Buffer::Interface has a rather complex API, so instead of
    // exposing it to the WASM, I'm cutting a corner here and copy
    // the data, so I can give to WASM a contigous array of data.
    std::vector<uint8_t> copy(buf.length(), 0);
    buf.copyOut(0, buf.length(), static_cast<void*>(copy.data()));
    absl::Span<uint8_t> data(copy);
    int32_t status;

    // We send the task to call WASM module in Hyperlight to the
    // guest_dispatcher_ thread, because we cannot afford blocking
    // this thread as it might have other things to do.
    //
    // For details refer to the explanation of the threading model
    // at the top of the file.
    {
      std::unique_lock<std::mutex> lock(mux_);
      guest_calls_.push([this, &status, data]() { status = sandbox_->run(data); });
    }
    cond_.notify_all();

    while (true) {
      {
        std::unique_lock<std::mutex> lock(mux_);
        // We wait for at least one of the following to happen:
        // 1. WASM in hyperlight finished executing the task we
        //    gave it above - in this case the task will be
        //    removed from the guest_calls_ queue.
        // 2. WASM in hyperlight called the host to do something
        //    - in this case the tasks it wants us to do will be
        //    in the host_calls_ queue and it will not be empty.
        cond_.wait(lock, [this]() -> bool { return !host_calls_.empty() || guest_calls_.empty(); });
      }

      bool had_work = false;
      bool exit = false;

      {
        std::unique_lock<std::mutex> lock(mux_);
        had_work = !host_calls_.empty();
        while (!host_calls_.empty()) {
          auto call = host_calls_.front();
          lock.unlock();

          // Unlike in guestCallDispatchLoop above, we don't have
          // to release the mutex to avoid deadlocks here. However
          // if we don't need to hold the mutex, we probably should
          // not hold it.
          call();

          lock.lock();
          host_calls_.pop();
        }
        exit = guest_calls_.empty();
      }

      if (had_work) {
        cond_.notify_all();
      }
      if (exit) {
        break;
      }
    }

    buf.drain(buf.length());
    ENVOY_LOG(info, "hyperlight filter finished work with status {}", status);
  }
  return Network::FilterStatus::StopIteration;
}

void HyperlightFilter::write(absl::Span<uint8_t> data) {
  if (read_callbacks_) {
    ::Envoy::Buffer::OwnedImpl buf(data.data(), data.size());
    read_callbacks_->connection().write(buf, false);
  } else {
    ENVOY_LOG(error, "read callbacks hasn't been set in the hyperlight filter");
  }
}

absl::Status HyperlightFilter::setupSandbox(const std::string& module_path) {
  auto builder_or = Builder::New();
  if (!builder_or.ok()) {
    ENVOY_LOG(error, "failed to create hyperlight sandbox builder: {}", builder_or.status());
    return builder_or.status();
  }

  std::unique_ptr<Builder> builder = std::move(builder_or).value();
  module_path_ = module_path;
  builder->setModulePath(module_path_);
  builder->registerFunction("write", [this](absl::Span<uint8_t> data) -> int32_t {
    std::unique_lock<std::mutex> lock(mux_);
    host_calls_.push([this, data]() { write(data); });
    lock.unlock();

    cond_.notify_all();

    lock.lock();
    cond_.wait(lock, [this]() -> bool { return host_calls_.empty(); });
    return 0;
  });

  auto sandbox_or = builder->build();
  if (!sandbox_or.ok()) {
    ENVOY_LOG(error, "failed to create hyperlight sandbox: {}", sandbox_or.status());
    return sandbox_or.status();
  }
  sandbox_ = std::move(sandbox_or).value();
  builder_.reset(builder.release());
  return absl::OkStatus();
}

} // namespace Hyperlight
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
