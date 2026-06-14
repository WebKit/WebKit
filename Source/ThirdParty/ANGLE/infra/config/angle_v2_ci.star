# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CI ANGLE builders using the angle_v2 recipe."""

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gardener_rotations.star", "gardener_rotations")
load("@chromium-luci//gn_args.star", "gn_args")
load("//constants.star", "default_experiments", "siso")

ci.defaults.set(
    executable = "recipe:angle_v2/angle_v2",
    builder_group = "ci",
    bucket = "ci",
    pool = "luci.chromium.gpu.ci",
    triggered_by = ["main-poller"],
    build_numbers = True,
    contact_team_email = "chrome-gpu-infra@google.com",
    gardener_rotations = gardener_rotations.rotation("angle", None, None),
    service_account = "angle-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    shadow_service_account = "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_project = siso.project.DEFAULT_TRUSTED,
    shadow_siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
    thin_tester_cores = 2,
    builderless = True,
    experiments = default_experiments,
    test_presentation = resultdb.test_presentation(
        column_keys = ["v.gpu"],
        grouping_keys = ["status", "v.test_suite"],
    ),
)

################################################################################
# Parent Builders                                                              #
################################################################################

def angle_linux_parent_builder(**kwargs):
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.LINUX_DEFAULT)
    ci.builder(**kwargs)

def angle_mac_parent_builder(**kwargs):
    kwargs.setdefault("cpu", "arm64")
    kwargs.setdefault("os", os.MAC_DEFAULT)
    ci.builder(**kwargs)

def angle_win_parent_builder(**kwargs):
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.WINDOWS_DEFAULT)
    kwargs.setdefault("ssd", None)
    ci.builder(**kwargs)

angle_linux_parent_builder(
    name = "angle-android-arm64-builder-rel",
    description_html = "Compiles release ANGLE test binaries for Android/arm64",
    schedule = "triggered",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "angle_v2_android",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_clang",
            "android_static_analysis",
            "arm64",
            "capture",
            "component",
            "opencl",
            "release_with_dchecks",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "compile|android|arm64",
        short_name = "rel",
    ),
)

angle_linux_parent_builder(
    name = "angle-linux-x64-builder-dbg",
    description_html = "Compiles debug ANGLE test binaries for Linux/x64",
    schedule = "triggered",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "component",
            "debug",
            "linux_clang",
            "opencl",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "compile|linux|x64",
        short_name = "dbg",
    ),
)

angle_linux_parent_builder(
    name = "angle-linux-x64-builder-rel",
    description_html = "Compiles release ANGLE test binaries for Linux/x64",
    schedule = "triggered",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "capture",
            "component",
            "linux_clang",
            "opencl",
            "release_with_dchecks",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "compile|linux|x64",
        short_name = "rel",
    ),
)

angle_mac_parent_builder(
    name = "angle-mac-arm64-builder-rel",
    description_html = "Compiles release ANGLE test binaries for Mac/arm64",
    schedule = "triggered",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "capture",
            "component",
            "mac_clang",
            "release_with_dchecks",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "compile|mac|arm64",
        short_name = "rel",
    ),
)

angle_mac_parent_builder(
    name = "angle-mac-x64-builder-rel",
    description_html = "Compiles release ANGLE test binaries for Mac/x64",
    schedule = "triggered",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "capture",
            "component",
            "mac_clang",
            "release_with_dchecks",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "compile|mac|x64",
        short_name = "rel",
    ),
)

angle_win_parent_builder(
    name = "angle-win-x64-builder-rel",
    description_html = "Compiles release ANGLE test binaries for Win/x64",
    schedule = "triggered",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "capture",
            "component",
            "opencl",
            "release_with_dchecks",
            "win_clang",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "compile|win|x64",
        short_name = "rel",
    ),
)

angle_win_parent_builder(
    name = "angle-win-x86-builder-rel",
    description_html = "Compiles release ANGLE test binaries for Win/x86",
    schedule = "triggered",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "component",
            "opencl",
            "release_with_dchecks",
            "win_clang",
            "x86",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "compile|win|x86",
        short_name = "rel",
    ),
)

################################################################################
# Child Testers                                                                #
################################################################################

ci.thin_tester(
    name = "angle-android-arm64-google-pixel4-rel",
    description_html = "Tests release ANGLE on Android/arm64 on Pixel 4 devices",
    parent = "angle-android-arm64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2_android",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|android|arm64|rel",
        short_name = "p4",
    ),
)

ci.thin_tester(
    name = "angle-android-arm64-google-pixel6-exp-rel",
    description_html = "Tests release ANGLE on Android/arm64 on experimental Pixel 6 devices",
    parent = "angle-android-arm64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2_android",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "test|android|arm64|rel|exp",
        short_name = "p6",
    ),
    list_view = "exp",
)

ci.thin_tester(
    name = "angle-android-arm64-google-pixel6-rel",
    description_html = "Tests release ANGLE on Android/arm64 on Pixel 6 devices",
    parent = "angle-android-arm64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2_android",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|android|arm64|rel",
        short_name = "p6",
    ),
)

ci.thin_tester(
    name = "angle-android-arm64-google-pixel10-rel",
    description_html = "Tests release ANGLE on Android/arm64 on Pixel 10 devices",
    parent = "angle-android-arm64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2_android",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|android|arm64|rel",
        short_name = "p10",
    ),
)

ci.thin_tester(
    name = "angle-android-arm64-samsung-s24-rel",
    description_html = "Tests release ANGLE on Android/arm64 on Samsung S24 devices",
    parent = "angle-android-arm64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2_android",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|android|arm64|rel",
        short_name = "s24",
    ),
)

ci.thin_tester(
    name = "angle-linux-x64-amd-rx5500xt-rel",
    description_html = "Tests release ANGLE on Linux/x64 on AMD RX 5500 XT GPUs",
    parent = "angle-linux-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|linux|x64|rel",
        short_name = "5500",
    ),
)

ci.thin_tester(
    name = "angle-linux-x64-intel-uhd630-exp-rel",
    description_html = "Tests release ANGLE on Linux/x64 on experimental Intel UHD 630 configs",
    parent = "angle-linux-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "test|linux|x64|rel|exp",
    #     short_name = "630",
    # ),
    list_view = "exp",
)

ci.thin_tester(
    name = "angle-linux-x64-intel-uhd630-rel",
    description_html = "Tests release ANGLE on Linux/x64 on Intel UHD 630 GPUs",
    parent = "angle-linux-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|linux|x64|rel",
        short_name = "630",
    ),
)

ci.thin_tester(
    name = "angle-linux-x64-nvidia-gtx1660-exp-rel",
    description_html = "Tests release ANGLE on Linux/x64 on experimental NVIDIA GTX 1660 configs",
    parent = "angle-linux-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "test|linux|x64|rel|exp",
    #     short_name = "1660",
    # ),
    list_view = "exp",
)

ci.thin_tester(
    name = "angle-linux-x64-nvidia-gtx1660-rel",
    description_html = "Tests release ANGLE on Linux/x64 on NVIDIA GTX 1660 GPUs",
    parent = "angle-linux-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|linux|x64|rel",
        short_name = "1660",
    ),
)

ci.thin_tester(
    name = "angle-linux-x64-sws-rel",
    description_html = "Tests release ANGLE on Linux/x64 with SwiftShader",
    parent = "angle-linux-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|linux|x64|rel",
        short_name = "sws",
    ),
)

ci.thin_tester(
    name = "angle-mac-arm64-apple-m2-rel",
    description_html = "Tests release ANGLE on Mac/arm64 on Apple M2 SoCs",
    parent = "angle-mac-arm64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|mac|arm64|rel",
        short_name = "m2",
    ),
)

ci.thin_tester(
    name = "angle-mac-x64-amd-5300m-exp-rel",
    description_html = "Tests release ANGLE on Mac/x64 on experimental configs of 16\" 2019 Macbook Pros w/ 5300M GPUs",
    parent = "angle-mac-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "test|mac|x64|rel|exp",
    #     short_name = "5300m",
    # ),
    list_view = "exp",
)

ci.thin_tester(
    name = "angle-mac-x64-amd-5300m-rel",
    description_html = "Tests release ANGLE on Mac/x64 on 16\" 2019 Macbook Pros w/ 5300M GPUs",
    parent = "angle-mac-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|mac|x64|rel",
        short_name = "5300m",
    ),
)

ci.thin_tester(
    name = "angle-mac-x64-amd-555x-rel",
    description_html = "Tests release ANGLE on Mac/x64 on 15\" 2019 Macbook Pros w/ Radeon Pro 555X GPUs",
    parent = "angle-mac-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|mac|x64|rel",
        short_name = "555x",
    ),
)

ci.thin_tester(
    name = "angle-mac-x64-intel-uhd630-exp-rel",
    description_html = "Tests release ANGLE on Mac/x64 on experimental configs of 2018 Mac Minis w/ Intel UHD 630 GPUs",
    parent = "angle-mac-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "test|mac|x64|rel|exp",
        short_name = "630",
    ),
    list_view = "exp",
)

ci.thin_tester(
    name = "angle-mac-x64-intel-uhd630-rel",
    description_html = "Tests release ANGLE on Mac/x64 on 2018 Mac Minis w/ Intel UHD 630 GPUs",
    parent = "angle-mac-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|mac|x64|rel",
        short_name = "630",
    ),
)

ci.thin_tester(
    name = "angle-win-x64-intel-uhd630-exp-rel",
    description_html = "Tests release ANGLE on Win/x64 on experimental configs of Intel UHD 630 GPUs",
    parent = "angle-win-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "test|win|x64|rel|exp",
    #     short_name = "630",
    # ),
    list_view = "exp",
)

ci.thin_tester(
    name = "angle-win-x64-intel-uhd630-rel",
    description_html = "Tests release ANGLE on Win/x64 on Intel UHD 630 GPUs",
    parent = "angle-win-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|win|x64|rel",
        short_name = "630",
    ),
)

ci.thin_tester(
    name = "angle-win-x64-intel-uhd770-rel",
    description_html = "Tests release ANGLE on Win/x64 on Intel UHD 770 GPUs",
    parent = "angle-win-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|win|x64|rel",
        short_name = "770",
    ),
)

ci.thin_tester(
    name = "angle-win-x64-nvidia-gtx1660-exp-rel",
    description_html = "Tests release ANGLE on Win/x64 on experimental configs of NVIDIA GTX 1660 GPUs",
    parent = "angle-win-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "test|win|x64|rel|exp",
    #     short_name = "1660",
    # ),
    list_view = "exp",
)

ci.thin_tester(
    name = "angle-win-x64-nvidia-gtx1660-rel",
    description_html = "Tests release ANGLE on Win/x64 on NVIDIA GTX 1660 GPUs",
    parent = "angle-win-x64-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|win|x64|rel",
        short_name = "1660",
    ),
)

ci.thin_tester(
    name = "angle-win-x86-sws-rel",
    description_html = "Tests release ANGLE on Win/x86 with SwiftShader",
    parent = "angle-win-x86-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "angle_v2",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "test|win|x86|rel",
        short_name = "sws",
    ),
)

################################################################################
# Trace Tests                                                                  #
################################################################################

angle_linux_parent_builder(
    name = "angle-linux-x64-trace",
    description_html = "Runs ANGLE GLES trace tests on Linux/x64 with SwiftShader",
    schedule = "triggered",
    properties = {
        "run_trace_tests": True,
    },
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "angle_v2_nointernal",
        ),
        chromium_config = builder_config.chromium_config(
            config = "angle_v2_clang",
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    # These GN args are not actually used since the trace tests do compilation
    # as part of running, but the recipe may try to "compile" as a side effect
    # of reusing the Chromium recipe code, so have some valid args.
    gn_args = gn_args.config(
        configs = [
            "linux_clang",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "trace|linux|x64",
        short_name = "rel",
    ),
)
