#pragma once

#include "envoy/network/filter.h"


namespace Envoy {
namespace Quic {

// findFilterChain is a helper function that adapts asynchronous interface of
// Network::FilterChainManager to a synchronous call. This method only works
// when searching for the filter chain cannot pause which should be the case
// with Quic listner filters.
const Network::FilterChain* findFilterChain(const Network::FilterChainManager& manager,
                                            Network::ConnectionSocket& socket,
                                            StreamInfo::StreamInfo& stream_info,
                                            Event::Dispatcher& dispatcher);

}  // namespace Quic
}  // namespace Envoy
