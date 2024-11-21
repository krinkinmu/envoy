#pragma once

#include <optional>

#include "envoy/network/filter.h"

namespace Envoy {
namespace Server {

std::optional<const Network::FilterChain*> findFilterChain(const Network::FilterChainManager& manager,
                                                           Network::ConnectionSocket& socket,
                                                           StreamInfo::StreamInfo& stream_info,
                                                           Event::Dispatcher& dispatcher);

}  // namespace Server
}  // namespace Envoy
