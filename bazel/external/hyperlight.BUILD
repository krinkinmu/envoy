load("@rules_rust//rust:defs.bzl", "rust_library")

licenses(["notice"])  # I would imagne it will be once the code is published

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "hyperlight_lib",
    srcs = select({
        ":opt-build": [":target/release/libhyperlight.a"],
        "//conditions:default": [":target/debug/libhyperlight.a"],
    }),
)

config_setting(
    name = "opt-build",
    values = {"compilation_mode": "opt"},
)
