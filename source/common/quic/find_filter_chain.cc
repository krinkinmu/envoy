#include "source/common/quic/find_filter_chain.h"

#include <optional>


namespace Envoy {
namespace Quic {

namespace {

class FindFilterHelper : public Network::FilterChainManagerCallbacks {
public:
  FindFilterHelper(StreamInfo::StreamInfo& info,
                   Network::ConnectionSocket& socket,
                   Event::Dispatcher& dispatcher)
    : stream_info_(info), socket_(socket), dispatcher_(dispatcher) {}

  StreamInfo::StreamInfo& streamInfo() override { return stream_info_; }
  Network::ConnectionSocket& socket() override { return socket_; }
  Event::Dispatcher& dispatcher() override { return dispatcher_; }

  void newConnectionWithFilterChain(const Network::FilterChain* filter_chain) override {
    filter_chain_ = filter_chain;
  }

  const Network::FilterChain* filterChain() const {
    ENVOY_BUG(filter_chain_.has_value(), "quic filter chain matching paused, which should never happen.");
    return *filter_chain_;
  }

private:
  StreamInfo::StreamInfo& stream_info_;
  Network::ConnectionSocket& socket_;
  Event::Dispatcher& dispatcher_;

  // We want to distinguish between filter manager returning nullptr and
  // filter manager not finishing at all.
  std::optional<const Network::FilterChain*> filter_chain_{std::nullopt};
};

}  // namespace

// findFilterChain is a helper function that adapts asynchronous interface of
// Network::FilterChainManager to a synchronous call. This method only works
// when searching for the filter chain cannot pause which should be the case
// with Quic listner filters.
const Network::FilterChain* findFilterChain(const Network::FilterChainManager& manager,
                                            Network::ConnectionSocket& socket,
                                            StreamInfo::StreamInfo& stream_info,
                                            Event::Dispatcher& dispatcher) {
  FindFilterHelper helper{stream_info, socket, dispatcher};
  manager.findFilterChain(&helper);
  return helper.filterChain();
}

}  // namespace Quic
}  // namespace Envoy
