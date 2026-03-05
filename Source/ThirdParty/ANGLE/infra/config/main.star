#!/usr/bin/env lucicfg
#
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# main.star: lucicfg configuration for ANGLE's standalone builders.

"""
main.star: lucicfg configuration for Angle's standalone builders.
"""

load("@chromium-luci//builders.star", "os")
load("@chromium-luci//chromium_luci.star", "chromium_luci")

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "builders/*/*/*",
        "builders/*/*/*/*",
        "builders/gn_args_locations.json",
        "luci/commit-queue.cfg",
        "luci/cr-buildbucket.cfg",
        "luci/luci-logdog.cfg",
        "luci/luci-milo.cfg",
        "luci/luci-notify.cfg",
        "luci/luci-scheduler.cfg",
        "luci/project.cfg",
        "luci/realms.cfg",
        # TODO(crbug.com/475260235): Remove project.pyl once all builders are
        # defined src-side.
        "project.pyl",
    ],
    fail_on_warnings = True,
)

luci.project(
    name = "angle",
    config_dir = "luci",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.PROJECT_CONFIGS_READER,
                acl.LOGDOG_READER,
                acl.BUILDBUCKET_READER,
                acl.SCHEDULER_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = [
                acl.SCHEDULER_OWNER,
            ],
            groups = "project-angle-admins",
        ),
        acl.entry(
            roles = [
                acl.LOGDOG_WRITER,
            ],
            groups = "luci-logdog-angle-writers",
        ),
    ],
    bindings = [
        luci.binding(
            roles = "role/configs.validator",
            users = "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
        ),
        luci.binding(
            roles = "role/swarming.poolOwner",
            groups = ["project-angle-owners", "mdb/chrome-troopers"],
        ),
        luci.binding(
            roles = "role/swarming.poolViewer",
            groups = "all",
        ),
        # Allow any Angle build to trigger a test ran under testing accounts
        # used on shared chromium tester pools.
        luci.binding(
            roles = "role/swarming.taskServiceAccount",
            users = [
                "chromium-tester@chops-service-accounts.iam.gserviceaccount.com",
                "chrome-gpu-gold@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
    ],
)

chromium_luci.configure_project(
    name = "angle",
    is_main = True,
    platforms = {},
)

chromium_luci.configure_builder_health_indicators(
    unhealthy_period_days = 7,
    pending_time_p50_min = 20,
)

chromium_luci.configure_ci(
    test_results_bq_dataset_name = "chromium",
    resultdb_index_by_timestamp = True,
)

chromium_luci.configure_try(
    test_results_bq_dataset_name = "chromium",
    resultdb_index_by_timestamp = True,
)

chromium_luci.configure_builders(
    os_dimension_overrides = {
        os.LINUX_DEFAULT: os.LINUX_JAMMY,
        os.MAC_DEFAULT: os.MAC_15,
        os.WINDOWS_DEFAULT: os.WINDOWS_10,
    },
)

chromium_luci.configure_per_builder_outputs(
    root_dir = "builders",
)

chromium_luci.configure_recipe_experiments(
    # This can be removed once all builders use the chromium-luci wrappers for
    # creating builders instead of directly calling luci.builder().
    require_builder_wrappers = False,
)

# Swarming permissions
luci.realm(name = "pools/ci")
luci.realm(name = "pools/try")

# Allow Angle owners and Chrome troopers to run tasks directly for testing and
# development on all Angle bots. E.g. via `led` tool or "Debug" button in Swarming Web UI.
luci.binding(
    realm = "@root",
    roles = "role/swarming.poolUser",
    groups = ["project-angle-owners", "mdb/chrome-troopers"],
)
luci.binding(
    realm = "@root",
    roles = "role/swarming.taskTriggerer",
    groups = ["project-angle-owners", "mdb/chrome-troopers"],
)

def _generate_project_pyl(ctx):
    ctx.output["project.pyl"] = "\n".join([
        "# This is a non-LUCI generated file",
        "# This is consumed by presubmit checks that need to validate the config",
        repr(dict(
            # We don't validate matching source-side configs for simplicity.
            validate_source_side_specs_have_builder = False,
        )),
        "",
    ])

lucicfg.generator(_generate_project_pyl)

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra/OpenGL%20ES_RGB_June16.svg",
    bug_url_template = "https://bugs.chromium.org/p/angleproject/issues/entry?components=Infra",
)

luci.logdog(gs_bucket = "chromium-luci-logdog")

luci.bucket(
    name = "ci",
    acls = [
        acl.entry(
            acl.BUILDBUCKET_TRIGGERER,
            users = [
                "angle-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
    ],
)

luci.bucket(
    name = "try",
    acls = [
        acl.entry(
            acl.BUILDBUCKET_TRIGGERER,
            groups = [
                "project-angle-tryjob-access",
                "service-account-cq",
            ],
        ),
    ],
)

# Shadow buckets for LED jobs.
luci.bucket(
    name = "ci.shadow",
    shadows = "ci",
    constraints = luci.bucket_constraints(
        pools = ["luci.angle.ci", "luci.chromium.gpu.ci"],
    ),
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = [
                "mdb/chrome-build-access-sphinx",
                "mdb/chrome-troopers",
                "chromium-led-users",
            ],
            users = [
                "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        luci.binding(
            roles = "role/buildbucket.triggerer",
            users = [
                "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        # Allow ci builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            users = [
                "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
    ],
    dynamic = True,
)

luci.bucket(
    name = "try.shadow",
    shadows = "try",
    constraints = luci.bucket_constraints(
        pools = ["luci.angle.try", "luci.chromium.gpu.try"],
        service_accounts = [
            "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
        ],
    ),
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = [
                "mdb/chrome-build-access-sphinx",
                "mdb/chrome-troopers",
                "chromium-led-users",
            ],
            users = [
                "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        luci.binding(
            roles = "role/buildbucket.triggerer",
            users = [
                "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        # Allow try builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = [
                "project-angle-try-task-accounts",
                "project-angle-tryjob-access",
            ],
        ),
    ],
    dynamic = True,
)

luci.gitiles_poller(
    name = "main-poller",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/angle/angle",
    refs = [
        "refs/heads/main",
    ],
    schedule = "with 10s interval",
)

# Views

luci.console_view(
    name = "ci",
    title = "ANGLE CI Builders",
    repo = "https://chromium.googlesource.com/angle/angle",
)

luci.list_view(
    name = "exp",
    title = "ANGLE Experimental CI Builders",
)

luci.list_view(
    name = "try",
    title = "ANGLE Try Builders",
)

luci.list_view_entry(
    list_view = "try",
    builder = "try/presubmit",
)

# Run other non-builder setup.
exec("//gn_args.star")
exec("//recipes.star")

# Handle any other builders defined in other files.
exec("//angle_v2_ci.star")
exec("//angle_v2_try.star")
exec("//legacy_builders.star")
