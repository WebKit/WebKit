# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CI ANGLE builders using the angle_v2 recipe."""

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//gardener_rotations.star", "gardener_rotations")
load("@chromium-luci//gn_args.star", "gn_args")
load("//constants.star", "default_experiments", "siso")

ci.defaults.set(
    executable = "recipe:angle_v2/angle_v2",
    builder_group = "angle",
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
    # TODO(crbug.com/475260235): Move this to the main view once it is at
    # parity with the older equivalent.
    list_view = "exp",
)

################################################################################
# Child Testers                                                                #
################################################################################

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
    # TODO(crbug.com/475260235): Move this to the main view once it is at
    # parity with the older equivalent.
    list_view = "exp",
)
