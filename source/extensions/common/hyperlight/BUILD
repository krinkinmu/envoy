load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

envoy_extension_package()

envoy_cc_library(
    name = "hyperlight_c_api",
    hdrs = [
        "hyperlight_c_api.h",
    ],
    deps = [
        "@com_github_hyperlight//:hyperlight_lib",
    ],
)

envoy_cc_library(
    name = "hyperlight_headers",
    hdrs = ["hyperlight.h"],
    deps = [
        "@com_google_absl//absl/status:statusor",
    ]
)

envoy_cc_extension(
    name = "hyperlight",
    srcs = ["hyperlight.cc"],
    visibility = [
        "//source/extensions:__subpackages__",
    ],
    deps = [
        ":hyperlight_headers",
	":hyperlight_c_api",
        "@com_google_absl//absl/status",
    ],
)
