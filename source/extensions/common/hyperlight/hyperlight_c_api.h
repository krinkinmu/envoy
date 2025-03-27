#pragma once

#include <cstdint>

struct Builder;
struct NativeBuilder;
struct LoadedWasmSandbox;
struct MultiUseSandbox;

extern "C" {
bool is_hypervisor_present(void);
bool native_is_hypervisor_present();

Builder* sandbox_builder_new(void);
NativeBuilder* native_sandbox_builder_new(void);

void sandbox_builder_free(Builder* builder);
void native_sandbox_builder_free(NativeBuilder* builder);

uint32_t sandbox_builder_set_module_path(Builder* builder, const char* module_path);
uint32_t native_sandbox_builder_set_module_path(NativeBuilder* builder, const char* module_path);

uint32_t sandbox_builder_register_host_function(Builder* builder, uint64_t cookie, const char* name,
                                                int32_t (*host_function)(uint64_t, uint8_t*,
                                                                         uint64_t));
uint32_t native_sandbox_builder_register_host_function(
  NativeBuilder* builder, uint64_t cookie, const char* name,
  int32_t (*host_function)(uint64_t, uint8_t*, uint64_t));

uint32_t sandbox_builder_build(Builder* builder, LoadedWasmSandbox** sandbox);
uint32_t native_sandbox_builder_build(NativeBuilder* builder, MultiUseSandbox** sandbox);

int32_t sandbox_run(LoadedWasmSandbox* sandbox, const uint8_t* data, uint64_t size);
int32_t native_sandbox_run(MultiUseSandbox* sandbox, const uint8_t* data, uint64_t size);

void sandbox_free(LoadedWasmSandbox* sandbox);
void native_sandbox_free(MultiUseSandbox* sandbox);
}
