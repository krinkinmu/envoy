#include "source/extensions/common/hyperlight/hyperlight.h"

#include "source/extensions/common/hyperlight/hyperlight_c_api.h"

#include "absl/status/status.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Hyperlight {

bool is_hypervisor_present() { return ::is_hypervisor_present(); }

Sandbox::Sandbox(::LoadedWasmSandbox* impl) : impl_(impl) {}

Sandbox::~Sandbox() {
  if (impl_ != nullptr) {
    ::sandbox_free(impl_);
    impl_ = nullptr;
  }
}

int32_t Sandbox::run(absl::Span<uint8_t> data) {
  return ::sandbox_run(impl_, data.data(), data.size());
}

absl::StatusOr<std::unique_ptr<Builder>> Builder::New() {
  return std::unique_ptr<Builder>(new Builder(::sandbox_builder_new()));
}

Builder::Builder(::Builder* impl) : impl_(impl) {}

Builder::~Builder() {
  if (impl_ != nullptr) {
    ::sandbox_builder_free(impl_);
    impl_ = nullptr;
  }
}

void Builder::setModulePath(const std::string& module_path) {
  ::sandbox_builder_set_module_path(impl_, module_path.c_str());
}

int32_t Builder::hostFunctionEntry(uint64_t cookie, uint8_t* data, uint64_t size) {
  Cookie* f = reinterpret_cast<Cookie*>(cookie);
  return f->run(absl::Span<uint8_t>(data, size));
}

void Builder::registerFunction(const std::string& name,
                               std::function<int32_t(absl::Span<uint8_t>)> function) {
  std::unique_ptr<Cookie> cookie = std::make_unique<Cookie>(std::move(function));
  uintptr_t id = reinterpret_cast<uintptr_t>(cookie.get());
  ::sandbox_builder_register_host_function(impl_, id, name.c_str(), &hostFunctionEntry);
  cookies_.emplace(id, std::move(cookie));
}

absl::StatusOr<std::unique_ptr<Sandbox>> Builder::build() {
  LoadedWasmSandbox* s = nullptr;
  uint32_t result = ::sandbox_builder_build(impl_, &s);
  if (result != 0) {
    return absl::InternalError("hyperlight wasm failed to construct proto sandbox");
  }
  return std::unique_ptr<Sandbox>(new Sandbox(s));
}

} // namespace Hyperlight
} // namespace Common
} // namespace Extensions
} // namespace Envoy
