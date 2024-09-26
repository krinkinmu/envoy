#include "envoy/common/exception.h"
#include "envoy/extensions/filters/network/hyperlight/v1/hyperlight.pb.h"
#include "envoy/extensions/filters/network/hyperlight/v1/hyperlight.pb.validate.h"
#include "envoy/registry/registry.h"

#include "source/common/common/logger.h"
#include "source/extensions/common/hyperlight/hyperlight.h"
#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/filters/network/hyperlight/hyperlight.h"


namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace Hyperlight {

using ::Envoy::Extensions::Common::Hyperlight::is_hypervisor_present;

class HyperlightConfigFactory
    : public Common::FactoryBase<envoy::extensions::filters::network::hyperlight::v1::Hyperlight>
    , Logger::Loggable<Logger::Id::filter> {
public:
  HyperlightConfigFactory() : FactoryBase("envoy.filters.network.hyperlight") {}

private:
  Network::FilterFactoryCb
  createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::network::hyperlight::v1::Hyperlight& config,
      Server::Configuration::FactoryContext&) override {
    if (!is_hypervisor_present()) {
      throwEnvoyExceptionOrPanic("hypervisor isn't present or unavailable");
    }

    std::string module_path = config.module_path();
    return [module_path = std::move(module_path)](Network::FilterManager& filter_manager) -> void {
      auto filter = std::make_shared<HyperlightFilter>();
      auto status = filter->setupSandbox(module_path);
      if (!status.ok()) {
        // I think that this lambda will be called in data path.
	// We are not allowed to throw exceptions in the data path
	// and we don't have any other ways to report an error.
	// So we log and fail open, e.g. we just skip this filter.
	// There are examples of other filters doing a similar thing.
	//
	// NOTE: Strictly speaking we report this filter as terminating
	// so it has to be the last filter in the chain and consume all
	// the data. So maybe a slightly better option here is to create
	// a simple filter that just silently eats the data.
        ENVOY_LOG(error, "failed to setup hyperlight WASM sandbox", status);
	return;
      }
      filter_manager.addReadFilter(std::move(filter));
    };
  }

  bool isTerminalFilterByProtoTyped(
      const envoy::extensions::filters::network::hyperlight::v1::Hyperlight&,
      Server::Configuration::ServerFactoryContext&) override {
    return true;
  }
};

REGISTER_FACTORY(HyperlightConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace Hyperlight
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
