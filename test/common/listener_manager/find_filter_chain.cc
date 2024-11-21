#include "test/common/listener_manager/find_filter_chain.h"


namespace Envoy {
namespace Server {

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

  std::optional<const Network::FilterChain*> filterChain() const {
    return filter_chain_;
  }

private:
  StreamInfo::StreamInfo& stream_info_;
  Network::ConnectionSocket& socket_;
  Event::Dispatcher& dispatcher_;

  std::optional<const Network::FilterChain*> filter_chain_{std::nullopt};
};

}  // namespace

std::optional<const Network::FilterChain*> findFilterChain(const Network::FilterChainManager& manager,
                                                           Network::ConnectionSocket& socket,
                                                           StreamInfo::StreamInfo& stream_info,
                                                           Event::Dispatcher& dispatcher) {
  FindFilterHelper helper{stream_info, socket, dispatcher};
  manager.findFilterChain(&helper);
  return helper.filterChain();
}

}  // namespace Server
}  // namespace Envoy
