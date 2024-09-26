load("@rules_rust//rust:defs.bzl", "rust_library")

licenses(["notice"])  # I would imagne it will be once the code is published

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "hyperlight_lib",
    srcs = [
        ":target/debug/libhyperlight.a",
    ],
)
