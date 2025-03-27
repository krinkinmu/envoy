licenses(["notice"])  # I would imagne it will be once the code is published

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "hyperlight_native_lib",
    srcs = select({
        ":opt-build": [":target/release/libhyperlight_native.a"],
	"//conditions:default": [":target/debug/libhyperlight_native.a"],
    }),
)

config_setting(
    name = "opt-build",
    values = {"compilation_mode": "opt"},
)
