#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Hyperlight {

bool is_hypervisor_present();

class Sandbox {
public:
  virtual ~Sandbox() {}

  virtual int32_t run(absl::Span<uint8_t> data) = 0;
};

class Builder {
public:
  virtual ~Builder() {}

  virtual void setModulePath(const std::string& name) = 0;
  virtual void registerFunction(const std::string& name,
		                std::function<int32_t(absl::Span<uint8_t>)> function) = 0;
  virtual absl::StatusOr<std::unique_ptr<Sandbox>> build() = 0;
};

absl::StatusOr<std::unique_ptr<Builder>> HyperlightWasmBuilder();
absl::StatusOr<std::unique_ptr<Builder>> HyperlightNativeBuilder();

} // namespace Hyperlight
} // namespace Common
} // namespace Extensions
} // namespace Envoy
