# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Try ANGLE builders using the angle_v2 recipe."""

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//try.star", "try_")
load("//constants.star", "default_experiments", "siso")

try_.defaults.set(
    executable = "recipe:angle_v2/angle_v2_trybot",
    builder_group = "try",
    bucket = "try",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    build_numbers = True,
    list_view = "try",
    cq_group = "main",
    contact_team_email = "chrome-gpu-infra@google.com",
    service_account = "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
    experiments = default_experiments,
    builder_config_settings = builder_config.try_settings(
        analyze_names = [
            "angle",
        ],
        retry_failed_shards = False,
        retry_without_patch = False,
    ),
)

################################################################################
# CQ Builders                                                                  #
################################################################################

## Templates

def apply_cq_builder_defaults(kwargs):
    kwargs.setdefault("max_concurrent_builds", 4)
    kwargs.setdefault("cq_settings", try_.cq_settings())
    return kwargs

def apply_linux_cq_builder_defaults(kwargs):
    """Applies default builder settings for a Linux CQ builder.

    Args:
        kwargs: The args being used for the builder as a dict.

    Returns:
        |kwargs| with default values set for a Linux CQ builder.
    """
    kwargs = apply_cq_builder_defaults(kwargs)
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.LINUX_DEFAULT)
    kwargs.setdefault("ssd", None)
    return kwargs

def apply_mac_cq_builder_defaults(kwargs):
    """Applies default builder settings for a Mac CQ builder.

    Args:
        kwargs: The args being used for the builder as a dict.

    Returns:
        |kwargs| with default values set for a Mac CQ builder.
    """
    kwargs = apply_cq_builder_defaults(kwargs)
    kwargs.setdefault("cpu", "arm64")
    kwargs.setdefault("os", os.MAC_DEFAULT)
    return kwargs

def apply_win_cq_builder_defaults(kwargs):
    """Applies default builder settings for a Windows CQ builder.

    Args:
        kwargs: The args being used for the builder as a dict.

    Returns:
        |kwargs| with default values set for a Windows CQ builder.
    """
    kwargs = apply_cq_builder_defaults(kwargs)
    kwargs.setdefault("os", os.WINDOWS_DEFAULT)
    kwargs.setdefault("ssd", None)
    return kwargs

def angle_linux_functional_cq_tester(**kwargs):
    kwargs = apply_linux_cq_builder_defaults(kwargs)
    try_.builder(**kwargs)

def angle_mac_functional_cq_tester(**kwargs):
    kwargs = apply_mac_cq_builder_defaults(kwargs)
    try_.builder(**kwargs)

def angle_win_functional_cq_tester(**kwargs):
    kwargs = apply_win_cq_builder_defaults(kwargs)
    try_.builder(**kwargs)

## Functional testers

angle_linux_functional_cq_tester(
    name = "angle-cq-android-arm64-rel",
    description_html = "Tests release ANGLE on Android/arm64 on multiple hardware configs. Blocks CL submission.",
    mirrors = [
        "ci/angle-android-arm64-builder-rel",
        "ci/angle-android-arm64-google-pixel4-rel",
        "ci/angle-android-arm64-google-pixel6-rel",
    ],
    gn_args = "ci/angle-android-arm64-builder-rel",
)

angle_linux_functional_cq_tester(
    name = "angle-cq-linux-x64-dbg",
    description_html = "Compiles all debug ANGLE targets for Linux/x64. Blocks CL submission.",
    mirrors = [
        "ci/angle-linux-x64-builder-dbg",
    ],
    gn_args = "ci/angle-linux-x64-builder-dbg",
)

angle_linux_functional_cq_tester(
    name = "angle-cq-linux-x64-rel",
    description_html = "Tests release ANGLE on Linux/x64 on multiple hardware configs. Blocks CL submission.",
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-intel-uhd630-rel",
        "ci/angle-linux-x64-nvidia-gtx1660-rel",
        "ci/angle-linux-x64-sws-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
)

angle_mac_functional_cq_tester(
    name = "angle-cq-mac-arm64-rel",
    description_html = "Tests release ANGLE on Mac/arm64 on multiple hardware configs. Blocks CL submission.",
    mirrors = [
        "ci/angle-mac-arm64-apple-m2-rel",
        "ci/angle-mac-arm64-builder-rel",
    ],
    gn_args = "ci/angle-mac-arm64-builder-rel",
)

angle_mac_functional_cq_tester(
    name = "angle-cq-mac-x64-rel",
    description_html = "Tests release ANGLE on Mac/x64 on multiple hardware configs. Blocks CL submission.",
    mirrors = [
        "ci/angle-mac-x64-amd-5300m-rel",
        "ci/angle-mac-x64-amd-555x-rel",
        "ci/angle-mac-x64-builder-rel",
        "ci/angle-mac-x64-intel-uhd630-rel",
    ],
    gn_args = "ci/angle-mac-x64-builder-rel",
)

angle_win_functional_cq_tester(
    name = "angle-cq-win-x64-rel",
    description_html = "Tests release ANGLE on Win/x64 on multiple hardware configs. Blocks CL submission.",
    mirrors = [
        "ci/angle-win-x64-builder-rel",
        "ci/angle-win-x64-intel-uhd630-rel",
        "ci/angle-win-x64-nvidia-gtx1660-rel",
    ],
    gn_args = "ci/angle-win-x64-builder-rel",
)

angle_win_functional_cq_tester(
    name = "angle-cq-win-x86-rel",
    description_html = "Tests release ANGLE on Win/x86 on multiple hardware configs. Blocks CL submission.",
    mirrors = [
        "ci/angle-win-x86-builder-rel",
        "ci/angle-win-x86-sws-rel",
    ],
    gn_args = "ci/angle-win-x86-builder-rel",
)

################################################################################
# Optional Builders                                                            #
################################################################################

## Templates

def apply_trace_tester_defaults(kwargs):
    kwargs.setdefault(
        "cq_settings",
        try_.cq_settings(
            # Trace tests are only run on CQ if files in the capture folders change.
            location_filters = [
                cq.location_filter(path_regexp = "DEPS"),
                cq.location_filter(path_regexp = "src/libANGLE/capture/.+"),
                cq.location_filter(path_regexp = "src/tests/angle_end2end_tests_expectations.txt"),
                cq.location_filter(path_regexp = "src/tests/capture.+"),
                cq.location_filter(path_regexp = "src/tests/egl_tests/.+"),
                cq.location_filter(path_regexp = "src/tests/gl_tests/.+"),
            ],
        ),
    )
    return kwargs

def angle_linux_trace_tester(**kwargs):
    kwargs = apply_trace_tester_defaults(kwargs)
    angle_linux_functional_cq_tester(**kwargs)

## Trace testers

angle_linux_trace_tester(
    name = "angle-cq-linux-x64-trace",
    description_html = "Runs ANGLE GLES trace tests on Linux/x64 with SwiftShader. Blocks CL submission.",
    mirrors = [
        "ci/angle-linux-x64-trace",
    ],
    properties = {
        "run_trace_tests": True,
    },
    gn_args = "ci/angle-linux-x64-trace",
)

################################################################################
# Manual Trybots                                                               #
################################################################################

## Templates

def angle_linux_manual_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        max_concurrent_builds = 1,
        cores = 8,
        os = os.LINUX_DEFAULT,
        ssd = None,
        **kwargs
    )

def angle_mac_manual_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        max_concurrent_builds = 1,
        cpu = "arm64",
        os = os.MAC_DEFAULT,
        **kwargs
    )

def angle_win_manual_builder(*, name, **kwargs):
    return try_.builder(
        name = name,
        max_concurrent_builds = 1,
        os = os.WINDOWS_DEFAULT,
        ssd = None,
        **kwargs
    )

## Functional testers

# This is effectively a copy of angle-cq-android-arm64-rel, but manual-only and
# with the angle_ir GN arg config set. Mirroring is done in this way instead
# of having CI builders because we do not have a need for the CI builders and
# this keeps the tests in sync between the IR and non-IR builders.
angle_linux_manual_builder(
    name = "angle-try-android-arm64-ir-rel",
    description_html = ("Tests release ANGLE on Android/arm64 on multiple hardware configs using " +
                        "ANGLE's new intermediate representation for shaders. Manual only."),
    mirrors = [
        "ci/angle-android-arm64-builder-rel",
        "ci/angle-android-arm64-google-pixel4-rel",
        "ci/angle-android-arm64-google-pixel6-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "android_clang",
            "android_static_analysis",
            "angle_ir",
            "arm64",
            "capture",
            "component",
            "opencl",
            "release_with_dchecks",
        ],
    ),
)

angle_linux_manual_builder(
    name = "angle-try-android-arm64-google-pixel4-rel",
    description_html = "Tests release ANGLE on Android/arm64 on Pixel 4 devices. Manual only.",
    mirrors = [
        "ci/angle-android-arm64-builder-rel",
        "ci/angle-android-arm64-google-pixel4-rel",
    ],
    gn_args = "ci/angle-android-arm64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-android-arm64-google-pixel6-exp-rel",
    description_html = "Tests release ANGLE on Android/arm64 on experimental Pixel 6 devices. Manual only.",
    mirrors = [
        "ci/angle-android-arm64-builder-rel",
        "ci/angle-android-arm64-google-pixel6-exp-rel",
    ],
    gn_args = "ci/angle-android-arm64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-android-arm64-google-pixel6-rel",
    description_html = "Tests release ANGLE on Android/arm64 on Pixel 6 devices. Manual only.",
    mirrors = [
        "ci/angle-android-arm64-builder-rel",
        "ci/angle-android-arm64-google-pixel6-rel",
    ],
    gn_args = "ci/angle-android-arm64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-android-arm64-google-pixel10-rel",
    description_html = "Tests release ANGLE on Android/arm64 on Pixel 10 devices. Manual only.",
    mirrors = [
        "ci/angle-android-arm64-builder-rel",
        "ci/angle-android-arm64-google-pixel10-rel",
    ],
    gn_args = "ci/angle-android-arm64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-android-arm64-samsung-s24-rel",
    description_html = "Tests release ANGLE on Android/arm64 on Samsung S24 devices. Manual only.",
    mirrors = [
        "ci/angle-android-arm64-builder-rel",
        "ci/angle-android-arm64-samsung-s24-rel",
    ],
    gn_args = "ci/angle-android-arm64-builder-rel",
)

# This is effectively a copy of angle-cq-linux-x64-rel, but manual-only and
# with the angle_ir GN arg config set. Mirroring is done in this way instead
# of having CI builders because we do not have a need for the CI builders and
# this keeps the tests in sync between the IR and non-IR builders.
angle_linux_manual_builder(
    name = "angle-try-linux-x64-ir-rel",
    description_html = ("Tests release ANGLE on Linux/x64 on multiple hardware configs using " +
                        "ANGLE's new intermediate representation for shaders. Manual only."),
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-intel-uhd630-rel",
        "ci/angle-linux-x64-nvidia-gtx1660-rel",
        "ci/angle-linux-x64-sws-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "angle_ir",
            "capture",
            "component",
            "linux_clang",
            "opencl",
            "release_with_dchecks",
            "x64",
        ],
    ),
)

angle_linux_manual_builder(
    name = "angle-try-linux-x64-amd-rx5500xt-rel",
    description_html = "Tests release ANGLE on Linux/x64 on AMD RX 5500 XT GPUs. Manual only.",
    mirrors = [
        "ci/angle-linux-x64-amd-rx5500xt-rel",
        "ci/angle-linux-x64-builder-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-linux-x64-intel-uhd630-exp-rel",
    description_html = "Tests release ANGLE on Linux/x64 on experimental Intel UhD 630 configs. Manual only.",
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-intel-uhd630-exp-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-linux-x64-intel-uhd630-rel",
    description_html = "Tests release ANGLE on Linux/x64 on Intel UHD 630 GPUs. Manual only.",
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-intel-uhd630-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-linux-x64-nvidia-gtx1660-exp-rel",
    description_html = "Tests release ANGLE on Linux/x64 on experimental NVIDIA GTX 1660 configs. Manual only.",
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-nvidia-gtx1660-exp-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-linux-x64-nvidia-gtx1660-rel",
    description_html = "Tests release ANGLE on Linux/x64 on NVIDIA GTX 1660 GPUs. Manual only.",
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-nvidia-gtx1660-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
)

angle_linux_manual_builder(
    name = "angle-try-linux-x64-sws-rel",
    description_html = "Tests release ANGLE on Linux/x64 with SwiftShader. Manual only.",
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-sws-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
)

# This is effectively a copy of angle-cq-mac-x64-rel, but manual-only and
# with the angle_ir GN arg config set. Mirroring is done in this way instead
# of having CI builders because we do not have a need for the CI builders and
# this keeps the tests in sync between the IR and non-IR builders.
angle_mac_manual_builder(
    name = "angle-try-mac-x64-ir-rel",
    description_html = ("Tests release ANGLE on Mac/x64 on multiple hardware configs using " +
                        "ANGLE's new intermediate representation for shaders. Manual only."),
    mirrors = [
        "ci/angle-mac-x64-amd-5300m-rel",
        "ci/angle-mac-x64-amd-555x-rel",
        "ci/angle-mac-x64-builder-rel",
        "ci/angle-mac-x64-intel-uhd630-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "angle_ir",
            "capture",
            "component",
            "mac_clang",
            "release_with_dchecks",
            "x64",
        ],
    ),
)

angle_mac_manual_builder(
    name = "angle-try-mac-arm64-m2-rel",
    description_html = "Tests release ANGLE on Mac/arm64 on Apple M2 SoCs. Manual only.",
    mirrors = [
        "ci/angle-mac-arm64-apple-m2-rel",
        "ci/angle-mac-arm64-builder-rel",
    ],
    gn_args = "ci/angle-mac-arm64-builder-rel",
)

angle_mac_manual_builder(
    name = "angle-try-mac-x64-amd-5300m-exp-rel",
    description_html = "Tests release ANGLE on Mac/x64 on experimental configs of 16\" 2019 Macbook Pros w/ 5300M GPUs. Manual only.",
    mirrors = [
        "ci/angle-mac-x64-amd-5300m-exp-rel",
        "ci/angle-mac-x64-builder-rel",
    ],
    gn_args = "ci/angle-mac-x64-builder-rel",
)

angle_mac_manual_builder(
    name = "angle-try-mac-x64-amd-5300m-rel",
    description_html = "Tests release ANGLE on Mac/x64 on 16\" 2019 Macbook Pros w/ 5300M GPUs. Manual only.",
    mirrors = [
        "ci/angle-mac-x64-amd-5300m-rel",
        "ci/angle-mac-x64-builder-rel",
    ],
    gn_args = "ci/angle-mac-x64-builder-rel",
)

angle_mac_manual_builder(
    name = "angle-try-mac-x64-amd-555x-rel",
    description_html = "Tests release ANGLE on Mac/x64 on 15\" 2019 Macbook Pros w/ Radeon Pro 555X GPUs. Manual only.",
    mirrors = [
        "ci/angle-mac-x64-amd-555x-rel",
        "ci/angle-mac-x64-builder-rel",
    ],
    gn_args = "ci/angle-mac-x64-builder-rel",
)

angle_mac_manual_builder(
    name = "angle-try-mac-x64-intel-uhd630-exp-rel",
    description_html = "Tests release ANGLE on Mac/x64 on experimental configs of 2018 Mac Minis w/ Intel UHD 630 GPUs. Manual only.",
    mirrors = [
        "ci/angle-mac-x64-builder-rel",
        "ci/angle-mac-x64-intel-uhd630-exp-rel",
    ],
    gn_args = "ci/angle-mac-x64-builder-rel",
)

angle_mac_manual_builder(
    name = "angle-try-mac-x64-intel-uhd630-rel",
    description_html = "Tests release ANGLE on Mac/x64 on 2018 Mac Minis w/ Intel UHD 630 GPUs. Manual only.",
    mirrors = [
        "ci/angle-mac-x64-builder-rel",
        "ci/angle-mac-x64-intel-uhd630-rel",
    ],
    gn_args = "ci/angle-mac-x64-builder-rel",
)

# This is effectively a copy of angle-cq-win-x64-rel, but manual-only and
# with the angle_ir GN arg config set. Mirroring is done in this way instead
# of having CI builders because we do not have a need for the CI builders and
# this keeps the tests in sync between the IR and non-IR builders.
angle_win_manual_builder(
    name = "angle-try-win-x64-ir-rel",
    description_html = ("Tests release ANGLE on Win/x64 on multiple hardware configs using " +
                        "ANGLE's new intermediate representation for shaders. Manual only."),
    mirrors = [
        "ci/angle-win-x64-builder-rel",
        "ci/angle-win-x64-intel-uhd630-rel",
        "ci/angle-win-x64-nvidia-gtx1660-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "angle_ir",
            "capture",
            "component",
            "opencl",
            "release_with_dchecks",
            "win_clang",
            "x64",
        ],
    ),
)

angle_win_manual_builder(
    name = "angle-try-win-x64-intel-uhd630-exp-rel",
    description_html = "Tests release ANGLE on Win/x64 on experimental configs of Intel UHD 630 GPUs. Manual only.",
    mirrors = [
        "ci/angle-win-x64-builder-rel",
        "ci/angle-win-x64-intel-uhd630-exp-rel",
    ],
    gn_args = "ci/angle-win-x64-builder-rel",
)

angle_win_manual_builder(
    name = "angle-try-win-x64-intel-uhd770-rel",
    description_html = "Tests release ANGLE on Win/x64 on Intel UHD 770 GPUs. Manual only.",
    mirrors = [
        "ci/angle-win-x64-builder-rel",
        "ci/angle-win-x64-intel-uhd770-rel",
    ],
    gn_args = "ci/angle-win-x64-builder-rel",
)

angle_win_manual_builder(
    name = "angle-try-win-x64-intel-uhd630-rel",
    description_html = "Tests release ANGLE on Win/x64 on Intel UHD 630 GPUs. Manual only.",
    mirrors = [
        "ci/angle-win-x64-builder-rel",
        "ci/angle-win-x64-intel-uhd630-rel",
    ],
    gn_args = "ci/angle-win-x64-builder-rel",
)

angle_win_manual_builder(
    name = "angle-try-win-x64-nvidia-gtx1660-exp-rel",
    description_html = "Tests release ANGLE on Win/x64 on experimental configs of NVIDIA GTX 1660 GPUs. Manual only.",
    mirrors = [
        "ci/angle-win-x64-builder-rel",
        "ci/angle-win-x64-nvidia-gtx1660-exp-rel",
    ],
    gn_args = "ci/angle-win-x64-builder-rel",
)

angle_win_manual_builder(
    name = "angle-try-win-x64-nvidia-gtx1660-rel",
    description_html = "Tests release ANGLE on Win/x64 on NVIDIA GTX 1660 GPUs. Manual only.",
    mirrors = [
        "ci/angle-win-x64-builder-rel",
        "ci/angle-win-x64-nvidia-gtx1660-rel",
    ],
    gn_args = "ci/angle-win-x64-builder-rel",
)

angle_win_manual_builder(
    name = "angle-try-win-x86-sws-rel",
    description_html = "Tests release ANGLE on Win/x86 with SwiftShader. Manual only.",
    mirrors = [
        "ci/angle-win-x86-builder-rel",
        "ci/angle-win-x86-sws-rel",
    ],
    gn_args = "ci/angle-win-x86-builder-rel",
)
