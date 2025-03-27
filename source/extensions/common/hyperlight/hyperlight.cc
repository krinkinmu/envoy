#include "source/extensions/common/hyperlight/hyperlight.h"

#include "source/extensions/common/hyperlight/hyperlight_c_api.h"

#include "absl/status/status.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Hyperlight {

struct CallDispatcher {
  CallDispatcher(std::function<int32_t(absl::Span<uint8_t>)> f) : function(std::move(f)) {}

  CallDispatcher(const CallDispatcher&) = default;
  CallDispatcher(CallDispatcher&&) = default;
  CallDispatcher& operator=(const CallDispatcher&) = default;
  CallDispatcher& operator=(CallDispatcher&&) = default;

  int32_t run(absl::Span<uint8_t> data) { return function(data); }

  std::function<int32_t(absl::Span<uint8_t>)> function;
};


namespace {

int32_t hostFunctionEntry(uint64_t cookie, uint8_t* data, uint64_t size) {
  CallDispatcher* dispatcher = reinterpret_cast<CallDispatcher*>(cookie);
  return dispatcher->run(absl::Span<uint8_t>(data, size));
}

}

bool is_hypervisor_present() { return ::is_hypervisor_present(); }

class HyperlightWasmSandbox : public Sandbox {
public:
  HyperlightWasmSandbox(::LoadedWasmSandbox* sandbox,
                        std::map<uintptr_t, std::unique_ptr<CallDispatcher>> dispatchers)
    : impl_(sandbox), dispatchers_(std::move(dispatchers))
  {}
  ~HyperlightWasmSandbox() {
    if (impl_ != nullptr) {
      ::sandbox_free(impl_);
      impl_ = nullptr;
    }
  }

  int32_t run(absl::Span<uint8_t> data) override {
    return ::sandbox_run(impl_, data.data(), data.size());
  }

private:
  ::LoadedWasmSandbox *impl_;
  std::map<uintptr_t, std::unique_ptr<CallDispatcher>> dispatchers_;
};

class HyperlightNativeSandbox : public Sandbox {
public:
  HyperlightNativeSandbox(::MultiUseSandbox* sandbox,
                        std::map<uintptr_t, std::unique_ptr<CallDispatcher>> dispatchers)
    : impl_(sandbox), dispatchers_(std::move(dispatchers))
  {}

  ~HyperlightNativeSandbox() {
    if (impl_ != nullptr) {
      ::native_sandbox_free(impl_);
      impl_ = nullptr;
    }
  }

  int32_t run(absl::Span<uint8_t> data) override {
    return ::native_sandbox_run(impl_, data.data(), data.size());
  }

private:
  ::MultiUseSandbox *impl_;
  std::map<uintptr_t, std::unique_ptr<CallDispatcher>> dispatchers_;
};

class HyperlightWasmBuilderImpl : public Builder {
public:
  HyperlightWasmBuilderImpl(::Builder *impl) : impl_(impl) {}
  ~HyperlightWasmBuilderImpl() {
    if (impl_ != nullptr) {
      ::sandbox_builder_free(impl_);
      impl_ = nullptr;
    }
  }

  void setModulePath(const std::string& path) override {
    ::sandbox_builder_set_module_path(impl_, path.c_str());
  }

  void registerFunction(const std::string& name,
                        std::function<int32_t(absl::Span<uint8_t>)> function) override {
    std::unique_ptr<CallDispatcher> dispatcher = std::make_unique<CallDispatcher>(std::move(function));
    uintptr_t id = reinterpret_cast<uintptr_t>(dispatcher.get());
    ::sandbox_builder_register_host_function(impl_, id, name.c_str(), &hostFunctionEntry);
    dispatchers_.emplace(id, std::move(dispatcher));
  }

  absl::StatusOr<std::unique_ptr<Sandbox>> build() override {
    ::LoadedWasmSandbox* s = nullptr;
    uint32_t result = ::sandbox_builder_build(impl_, &s);
    if (result != 0) {
      return absl::InternalError("hyperlight wasm failed to construct proto sandbox");
    }
    return std::unique_ptr<Sandbox>(new HyperlightWasmSandbox(s, std::move(dispatchers_)));
  }

private:
  ::Builder *impl_ = nullptr;
  std::map<uintptr_t, std::unique_ptr<CallDispatcher>> dispatchers_;
};

class HyperlightNativeBuilderImpl : public Builder {
public:
  HyperlightNativeBuilderImpl(::NativeBuilder *impl) : impl_(impl) {}
  ~HyperlightNativeBuilderImpl() {
    if (impl_ != nullptr) {
      ::native_sandbox_builder_free(impl_);
      impl_ = nullptr;
    }
  }

  void setModulePath(const std::string& path) override {
    ::native_sandbox_builder_set_module_path(impl_, path.c_str());
  }

  void registerFunction(const std::string& name,
                        std::function<int32_t(absl::Span<uint8_t>)> function) override {
    std::unique_ptr<CallDispatcher> dispatcher = std::make_unique<CallDispatcher>(std::move(function));
    uintptr_t id = reinterpret_cast<uintptr_t>(dispatcher.get());
    ::native_sandbox_builder_register_host_function(impl_, id, name.c_str(), &hostFunctionEntry);
    dispatchers_.emplace(id, std::move(dispatcher));
  }

  absl::StatusOr<std::unique_ptr<Sandbox>> build() override {
    ::MultiUseSandbox* s = nullptr;
    uint32_t result = ::native_sandbox_builder_build(impl_, &s);
    if (result != 0) {
      return absl::InternalError("hyperlight native failed to construct sandbox");
    }
    return std::unique_ptr<Sandbox>(new HyperlightNativeSandbox(s, std::move(dispatchers_)));
  }

private:
  ::NativeBuilder *impl_ = nullptr;
  std::map<uintptr_t, std::unique_ptr<CallDispatcher>> dispatchers_;
};

absl::StatusOr<std::unique_ptr<Builder>> HyperlightWasmBuilder() {
  return std::unique_ptr<Builder>(new HyperlightWasmBuilderImpl(::sandbox_builder_new()));
}

absl::StatusOr<std::unique_ptr<Builder>> HyperlightNativeBuilder() {
  return std::unique_ptr<Builder>(new HyperlightNativeBuilderImpl(::native_sandbox_builder_new()));
}

} // namespace Hyperlight
} // namespace Common
} // namespace Extensions
} // namespace Envoy
