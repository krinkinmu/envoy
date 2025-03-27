// I think I need to explain how the threading model implemented
// here came to be, why it's bad and what should a better model
// do. Partially because it explains quite a few complexities in
// this code and partially because it reveals something about how
// Envoy works.
//
// Envoy creates a fixed number of worker threads during startup.
// Those threads are special in multiple ways:
//
// 1. They are configured in Envoy specific way that allows them to
//    call Envoy functions - a thread that wasn't set up in such
//    a way cannot, in general, call Envoy functions;
// 2. Connections that Envoy receives once assigend to a thread
//    stay with that thread - so a lot of processing related to
//    the same connection in Envoy stays in the same thread - on
//    the flip side, many data structures in Envoy are not protected
//    to be accessed concurrently from multiple threads because of
//    that;
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
// work together without some additional boilerplate:
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
// The current solution is to create a separate thread for calling
// into the Hyperlight. That allows us to address the first issue
// by quickly pushing work off the envoy thread and releasing it to
// do other work.
//
// For the host calls that hyperlight may need, we use Envoy
// Dispatcher::post method that allows to schedule work on Envoy thread
// from another, potentially non-Envoy thread.
#include "source/extensions/filters/network/hyperlight/hyperlight.h"

#include "envoy/buffer/buffer.h"
#include "envoy/common/exception.h"
#include "envoy/event/dispatcher.h"
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
  cond_.notify_one();
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

    bool exit = false;

    {
      std::unique_lock<std::mutex> lock(mux_);
      while (!guest_calls_.empty()) {
        auto call = guest_calls_.front();
        lock.unlock();

        // We can't hold the mutex while we are calling into
        // hyperlight. The WASM module that hyperlight
        // executes may call back into Envoy and if we hold
        // a mutex here it might result in a deadlock.
        call();

        lock.lock();
        guest_calls_.pop();
      }
      exit = guest_done_;
    }
    if (exit) {
      return;
    }
  }
}

// I don't think that it matters whether we continue processing
// or not here, since Hyperlight filter is terminal and there are
// no other plugins after it - there is nothing to continue.
//
// NOTE: it might be confusing, but pausing processing does not
// actually prevent any additional onData calls from being
// triggered in this filter, it only prevents onData calls from
// being triggered in filters after this one.
void HyperlightFilter::continueProcessing() {
  RELEASE_ASSERT(read_callbacks_, "read_callbacks_ is null unexpectedly");
  read_callbacks_->continueReading();
}

Network::FilterStatus HyperlightFilter::onNewConnection() {
  return Network::FilterStatus::Continue;
}

Network::FilterStatus HyperlightFilter::onData(Buffer::Instance& buf, bool) {
  //ENVOY_LOG(info, "hyperlight filter received {} bytes", buf.length());
  RELEASE_ASSERT(sandbox_, "sandbox has not been initialized");
  // Buffer::Interface has a rather complex API, so instead of
  // exposing it to the WASM, I'm cutting a corner here and copy
  // the data, so I can give to WASM a contigous array of data.
  std::vector<uint8_t> data(buf.length(), 0);
  buf.copyOut(0, buf.length(), static_cast<void*>(data.data()));
  buf.drain(buf.length());

  // We send the task to call WASM module in Hyperlight to the
  // guest_dispatcher_ thread, because we cannot afford blocking
  // this thread as it might have other things to do.
  //
  // For details refer to the explanation of the threading model
  // at the top of the file.
  {
    std::unique_lock<std::mutex> lock(mux_);
    guest_calls_.push([this, data = std::move(data)]() mutable {
      absl::Span<uint8_t> span(data);
      RELEASE_ASSERT(sandbox_->run(span) == 0, "guest call failed");
      continueProcessing();
    });
  }
  cond_.notify_one();
  return Network::FilterStatus::StopIteration;
}

void HyperlightFilter::write(absl::Span<uint8_t> data) {
  RELEASE_ASSERT(read_callbacks_, "read_callbacks_ is null unexpectedly");
  ::Envoy::Buffer::OwnedImpl buf(data.data(), data.size());
  read_callbacks_->connection().write(buf, false);
}

absl::Status HyperlightFilter::setupSandbox(const std::string& module_path, bool native) {
  auto builder_or = native ? Common::Hyperlight::HyperlightNativeBuilder()
	                   : Common::Hyperlight::HyperlightWasmBuilder();
  if (!builder_or.ok()) {
    ENVOY_LOG(error, "failed to create hyperlight sandbox builder: {}", builder_or.status());
    return builder_or.status();
  }

  std::unique_ptr<Builder> builder = std::move(builder_or).value();
  builder->setModulePath(module_path);
  builder->registerFunction("write", [this](absl::Span<uint8_t> data) -> int32_t {
    RELEASE_ASSERT(read_callbacks_, "read_callbacks_ is null unexpectedly");
    bool done = false;
    read_callbacks_->connection().dispatcher().post([this, data, &done]() {
      write(data);
      {
        std::unique_lock<std::mutex> lock(host_mux_);
        done = true;
      }
      host_cond_.notify_one();
    });
    std::unique_lock<std::mutex> lock(host_mux_);
    host_cond_.wait(lock, [&done]() -> bool { return done; });
    return 0;
  });

  auto sandbox_or = builder->build();
  if (!sandbox_or.ok()) {
    ENVOY_LOG(error, "failed to create hyperlight sandbox: {}", sandbox_or.status());
    return sandbox_or.status();
  }
  sandbox_ = std::move(sandbox_or).value();
  return absl::OkStatus();
}

} // namespace Hyperlight
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
