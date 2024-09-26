#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

struct Builder;
struct LoadedWasmSandbox;

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Hyperlight {

bool is_hypervisor_present();

class Sandbox {
public:
    ~Sandbox();

    Sandbox(const Sandbox&) = delete;
    Sandbox(Sandbox&&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;
    Sandbox& operator=(Sandbox&&) = delete;

    int32_t run(absl::Span<uint8_t> data);

private:
    Sandbox(LoadedWasmSandbox *impl);

    ::LoadedWasmSandbox *impl_;

    friend class Builder;
};

class Builder {
public:
    static absl::StatusOr<std::unique_ptr<Builder>> New();
    ~Builder();

    Builder(const Builder&) = delete;
    Builder(Builder&&) = delete;
    Builder& operator=(const Builder&) = delete;
    Builder& operator=(Builder&&) = delete;

    void registerFunction(
        const std::string& name,
	std::function<int32_t(absl::Span<uint8_t>)> function);
    void setModulePath(const std::string& name);
    absl::StatusOr<std::unique_ptr<Sandbox>> build();

private:
    Builder(::Builder *impl);

    struct Cookie {
        Cookie(std::function<int32_t(absl::Span<uint8_t>)> f) : function(std::move(f)) {}

	Cookie(const Cookie&) = default;
	Cookie(Cookie&&) = default;
	Cookie& operator=(const Cookie&) = default;
	Cookie& operator=(Cookie&&) = default;

	int32_t run(absl::Span<uint8_t> data) { return function(data); }

        std::function<int32_t(absl::Span<uint8_t>)> function;
    };

    static int32_t hostFunctionEntry(uint64_t cookie, uint8_t* data, uint64_t size);

    ::Builder *impl_;
    std::map<uintptr_t, std::unique_ptr<Cookie>> cookies_;
};

} // namespace Hyperlight
} // namespace Common
} // namespace Extensions
} // namespace Envoy
