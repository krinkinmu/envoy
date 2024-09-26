load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
)

licenses(["notice"])

envoy_extension_package()

envoy_cc_library(
    name = "hyperlight",
    srcs = ["hyperlight.cc"],
    hdrs = ["hyperlight.h"],
    deps = [
        "//envoy/buffer:buffer_interface",
	"//envoy/common:exception_lib",
        "//envoy/network:connection_interface",
        "//envoy/network:filter_interface",
	"//source/common/buffer:buffer_lib",
        "//source/common/common:assert_lib",
        "//source/common/common:minimal_logger_lib",
	"//source/extensions/common/hyperlight",
	"@com_google_absl//absl/status",
    ],
)

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    extra_visibility = [
        "//test/integration:__subpackages__",
    ],
    deps = [
        ":hyperlight",
	"//envoy/common:exception_lib",
        "//envoy/registry",
        "//envoy/server:filter_config_interface",
	"//source/extensions/common/hyperlight",
        "//source/extensions/filters/network/common:factory_base_lib",
        "@envoy_api//envoy/extensions/filters/network/hyperlight/v1:pkg_cc_proto",
    ],
)
