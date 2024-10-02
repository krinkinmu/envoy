#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "envoy/network/filter.h"

#include "source/common/common/logger.h"
#include "source/extensions/common/hyperlight/hyperlight.h"

#include "absl/status/status.h"
#include "absl/types/span.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace Hyperlight {

using ::Envoy::Extensions::Common::Hyperlight::Builder;
using ::Envoy::Extensions::Common::Hyperlight::Sandbox;

class HyperlightFilter : public Network::ReadFilter, Logger::Loggable<Logger::Id::filter> {
public:
  HyperlightFilter();
  ~HyperlightFilter() override;

  HyperlightFilter(const HyperlightFilter&) = delete;
  HyperlightFilter(HyperlightFilter&&) = delete;
  HyperlightFilter& operator=(const HyperlightFilter&) = delete;
  HyperlightFilter& operator=(HyperlightFilter&&) = delete;

  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;
  Network::FilterStatus onNewConnection() override;

  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    read_callbacks_ = &callbacks;
  }

  absl::Status setupSandbox(const std::string& module_path);

private:
  void write(absl::Span<uint8_t> data);
  void guestCallDispatchLoop();

  Network::ReadFilterCallbacks* read_callbacks_ = nullptr;

  // NOTE: The order of the fields here does matter.
  //
  // In C++ the order of construction and destruction
  // is defined by the order of the fields in the class.
  // So the fields that go earlier are constructed earlier
  // and destructed later.
  //
  // Because of this, when we construct guestDispatcher_
  // it will start a thread that will start accessing
  // fields of the class, so by that point all the fields
  // that could be touched in guestCallDispatchLoop directly
  // or indirectly have to be initialized before we start
  // the thread and, therefore, guestDispatcher_ should
  // basically be one of the last fields in the class.
  std::queue<std::function<void()>> guest_calls_;
  std::queue<std::function<void()>> host_calls_;
  bool guest_done_ = false;
  std::condition_variable cond_;
  std::mutex mux_;
  std::string module_path_;

  // NOTE: We have to keep builder around while Hyperlight
  // sandbox is still alive.
  //
  // The reason for that is Builder holds the data required
  // to dispatch callbacks from the WASM module running
  // inside Hyperlight into host. So as long as the sandbox
  // is still there and we might get some guest calls, we
  // should keep the builder around even if we don't use it
  // directly.
  std::unique_ptr<Builder> builder_;
  std::unique_ptr<Sandbox> sandbox_;
  std::thread guest_dispatcher_;
};

} // namespace Hyperlight
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
