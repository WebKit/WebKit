# Copyright 2026 The LibYuv Project Authors. All rights reserved.
#
# Shared target definitions for Bazel and Blaze builds.

def libyuv_srcs(prefix = ""):
    return native.glob(
        [
            prefix + "source/*.cc",
            prefix + "include/libyuv/*.h",
        ],
        exclude = [
            prefix + "source/*neon*.cc",
            prefix + "source/*sve*.cc",
            prefix + "source/*sme*.cc",
        ],
    )

def libyuv_hdrs(prefix = ""):
    return [
        prefix + "include/libyuv/compare.h",
        prefix + "include/libyuv/convert.h",
        prefix + "include/libyuv/convert_from_argb.h",
        prefix + "include/libyuv/cpu_id.h",
        prefix + "include/libyuv/row.h",
    ]

def libyuv_neon_srcs(prefix = ""):
    return native.glob([
        prefix + "source/*neon*.cc",
        prefix + "include/libyuv/*.h",
    ])

def libyuv_sve_srcs(prefix = ""):
    return native.glob([
        prefix + "source/*sve*.cc",
        prefix + "include/libyuv/*.h",
    ])

def libyuv_test_srcs(prefix = ""):
    return native.glob([
        prefix + "unit_test/*.cc",
        prefix + "unit_test/*.h",
    ])
