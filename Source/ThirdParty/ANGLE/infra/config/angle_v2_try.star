# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Try ANGLE builders using the angle_v2 recipe."""

load("@chromium-luci//builders.star", "os")
load("@chromium-luci//try.star", "try_")
load("//constants.star", "default_experiments", "siso")

try_.defaults.set(
    executable = "recipe:angle_v2/angle_v2_trybot",
    builder_group = "angle",
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
)

################################################################################
# CQ Builders                                                                  #
################################################################################

## Templates

def apply_cq_builder_defaults(kwargs):
    kwargs.setdefault("max_concurrent_builds", 4)
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

def angle_linux_functional_cq_tester(**kwargs):
    kwargs = apply_linux_cq_builder_defaults(kwargs)

    # TODO(crbug.com/475260235): Actually add the try_.job() entry when we are
    # ready to add chromium-luci builders to the CQ.
    try_.builder(**kwargs)

## Functional testers

angle_linux_functional_cq_tester(
    name = "angle-cq-linux-x64-rel",
    description_html = "Tests release ANGLE on Linux/x64 on multiple hardware configs. Blocks CL submission.",
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-sws-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
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

## Functional testers

angle_linux_manual_builder(
    name = "angle-try-linux-x64-sws-rel",
    description_html = "Tests release ANGLE on Linux/x64 with SwiftShader. Manual only.",
    mirrors = [
        "ci/angle-linux-x64-builder-rel",
        "ci/angle-linux-x64-sws-rel",
    ],
    gn_args = "ci/angle-linux-x64-builder-rel",
)
