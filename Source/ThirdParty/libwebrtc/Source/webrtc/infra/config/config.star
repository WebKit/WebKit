#!/usr/bin/env lucicfg

#  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# https://chromium.googlesource.com/infra/luci/luci-go/+/main/lucicfg/doc/

"""LUCI project configuration for WebRTC CQ and CI."""

load("@chromium-luci//chromium_luci.star", "chromium_luci")
load("@chromium-luci//recipe_experiments.star", "register_recipe_experiments")

WEBRTC_GIT = "https://webrtc.googlesource.com/src"
WEBRTC_GERRIT = "https://webrtc-review.googlesource.com/src"
WEBRTC_TROOPER_EMAIL = "webrtc-troopers-robots@google.com"

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

luci.builder.defaults.test_presentation.set(
    resultdb.test_presentation(grouping_keys = ["status", "v.test_suite"]),
)

lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "luci/commit-queue.cfg",
        "luci/cr-buildbucket.cfg",
        "luci/luci-analysis.cfg",
        "luci/luci-logdog.cfg",
        "luci/luci-milo.cfg",
        "luci/luci-notify.cfg",
        "luci/luci-notify/**/*",
        "luci/luci-scheduler.cfg",
        "luci/project.cfg",
        "luci/realms.cfg",
    ],
)

chromium_luci.configure_project(
    name = "project",
    is_main = True,
    platforms = {},
)

chromium_luci.configure_builder_health_indicators(
    unhealthy_period_days = 7,
    pending_time_p50_min = 20,
)

chromium_luci.configure_ci(
    test_results_bq_dataset_name = "resultdb",
    resultdb_index_by_timestamp = False,
)

chromium_luci.configure_recipe_experiments(
    # This can be removed once all builders use the chromium-luci wrappers for
    # creating builders instead of directly calling luci.builder().
    require_builder_wrappers = False,
)

luci.project(
    name = "webrtc",
    config_dir = "luci",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            [acl.BUILDBUCKET_READER, acl.LOGDOG_READER, acl.PROJECT_CONFIGS_READER, acl.SCHEDULER_READER],
            groups = ["all"],
        ),
        acl.entry(acl.LOGDOG_WRITER, groups = ["luci-logdog-chromium-writers"]),
        acl.entry(acl.SCHEDULER_OWNER, groups = ["project-webrtc-admins"]),
    ],
    bindings = [
        luci.binding(
            roles = "role/configs.validator",
            users = [
                "webrtc-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        luci.binding(
            roles = "role/swarming.poolOwner",
            groups = "project-webrtc-admins",
        ),
        luci.binding(
            roles = "role/swarming.poolViewer",
            groups = "all",
        ),
        # Allow any WebRTC build to trigger a test ran under chromium-tester@
        # task service account.
        luci.binding(
            roles = "role/swarming.taskServiceAccount",
            users = [
                "chromium-tester@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
        # Roles for LUCI Analysis.
        luci.binding(
            roles = "role/analysis.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/analysis.queryUser",
            groups = "authenticated-users",
        ),
        luci.binding(
            roles = "role/analysis.editor",
            groups = "googlers",
        ),
    ],
)

luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra/webrtc-logo-vert-retro-255x305.png",
)

# Configure Weetbix (config is copied verbatim)
################################################################################

lucicfg.emit(
    dest = "luci/luci-analysis.cfg",
    data = io.read_file("luci-analysis.cfg"),
)

################################################################################

luci.notify(tree_closing_enabled = True)

luci.cq(
    status_host = "chromium-cq-status.appspot.com",
    submit_max_burst = 1,
    submit_burst_delay = 1 * time.minute,
)

luci.gitiles_poller(
    name = "webrtc-gitiles-trigger-main",
    bucket = "ci",
    repo = WEBRTC_GIT,
    refs = ["refs/heads/main"],
)

# Swarming permissions:

luci.realm(name = "pools/cron", bindings = [
    # Unlike WebRTC's own builders, other projects need an explicit grant to use this pool.
    luci.binding(
        roles = "role/swarming.poolUser",
        projects = "libyuv",
    ),
])

luci.realm(name = "pools/ci")
luci.realm(name = "pools/ci-tests", bindings = [
    # Allow task service accounts of .ci pool/bucket to trigger tasks here.
    luci.binding(
        roles = "role/swarming.poolUser",
        groups = "project-webrtc-ci-task-accounts",
    ),
    # Allow tasks here to use .ci task service accounts.
    luci.binding(
        roles = "role/swarming.taskServiceAccount",
        groups = "project-webrtc-ci-task-accounts",
    ),
])
luci.realm(
    name = "ci",
    extends = "debug-bot-acls",
    bindings = [
        # Allow CI builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-webrtc-ci-task-accounts",
        ),
    ],
)

luci.realm(name = "pools/try", bindings = [
    # Allow to use LED & Swarming "Debug" feature to a larger group but only on try bots / builders.
    luci.binding(
        roles = "role/swarming.poolUser",
        groups = "project-webrtc-led-users",
    ),
])
luci.realm(name = "pools/try-tests", bindings = [
    # Allow task service accounts of .try pool/bucket to trigger tasks here.
    luci.binding(
        roles = "role/swarming.poolUser",
        groups = "project-webrtc-try-task-accounts",
    ),
    # Allow tasks here to use .try task service accounts.
    luci.binding(
        roles = "role/swarming.taskServiceAccount",
        groups = "project-webrtc-try-task-accounts",
    ),
])
luci.realm(
    name = "try",
    extends = "debug-bot-acls",
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = "project-webrtc-led-users",
        ),
        luci.binding(
            roles = "role/swarming.taskTriggerer",
            groups = "project-webrtc-led-users",
        ),
        # Allow try builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-webrtc-try-task-accounts",
        ),
    ],
)

luci.realm(name = "pools/perf", bindings = [
    # Allow to use LED & Swarming "Debug" feature to a larger group but only on perf bots / builders.
    luci.binding(
        roles = "role/swarming.poolUser",
        groups = "project-webrtc-led-users",
    ),
])
luci.realm(
    name = "perf",
    extends = "debug-bot-acls",
    bindings = [
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = "project-webrtc-led-users",
        ),
        luci.binding(
            roles = "role/swarming.taskTriggerer",
            groups = "project-webrtc-led-users",
        ),
    ],
)

# Allow admins to use LED & Swarming "Debug" feature on WebRTC bots where this permission is extended.
luci.realm(name = "debug-bot-acls", bindings = [
    luci.binding(
        roles = "role/swarming.poolUser",
        groups = "project-webrtc-admins",
    ),
    luci.binding(
        roles = "role/buildbucket.creator",
        groups = "project-webrtc-admins",
    ),
    luci.binding(
        roles = "role/swarming.taskTriggerer",
        groups = "project-webrtc-admins",
    ),
])

# Bucket definitions:

luci.bucket(
    name = "try",
    acls = [
        acl.entry(acl.BUILDBUCKET_TRIGGERER, groups = [
            "service-account-cq",
            "project-webrtc-tryjob-access",
        ]),
    ],
    constraints = luci.bucket_constraints(
        pools = ["luci.webrtc.try"],
        service_accounts = ["webrtc-try-builder@chops-service-accounts.iam.gserviceaccount.com"],
    ),
)

luci.bucket(
    name = "ci",
    acls = [
        acl.entry(acl.BUILDBUCKET_TRIGGERER, groups = [
            "project-webrtc-ci-schedulers",
        ]),
    ],
    constraints = luci.bucket_constraints(
        pools = ["luci.webrtc.ci"],
        service_accounts = ["webrtc-ci-builder@chops-service-accounts.iam.gserviceaccount.com"],
    ),
)

luci.bucket(
    name = "perf",
    acls = [
        acl.entry(acl.BUILDBUCKET_TRIGGERER, users = [
            "webrtc-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        ]),
        acl.entry(acl.BUILDBUCKET_TRIGGERER, groups = [
            # Allow Pinpoint to trigger builds for bisection
            "service-account-chromeperf",
        ]),
    ],
    constraints = luci.bucket_constraints(
        pools = ["luci.webrtc.perf"],
        service_accounts = ["webrtc-ci-builder@chops-service-accounts.iam.gserviceaccount.com"],
    ),
)

luci.bucket(
    name = "cron",
)

# Commit queue definitions:

luci.cq_group(
    name = "cq",
    tree_status_host = "webrtc-status.appspot.com",
    watch = [cq.refset(repo = WEBRTC_GERRIT, refs = ["refs/heads/main"])],
    acls = [
        acl.entry(acl.CQ_COMMITTER, groups = ["project-webrtc-submit-access"]),
        acl.entry(acl.CQ_DRY_RUNNER, groups = ["project-webrtc-tryjob-access"]),
    ],
    allow_owner_if_submittable = cq.ACTION_DRY_RUN,
    retry_config = cq.RETRY_ALL_FAILURES,
    cancel_stale_tryjobs = True,
)

luci.cq_group(
    name = "cq_branch",
    watch = [cq.refset(repo = WEBRTC_GERRIT, refs = ["refs/branch-heads/.+"])],
    acls = [
        acl.entry(acl.CQ_COMMITTER, groups = ["project-webrtc-submit-access"]),
        acl.entry(acl.CQ_DRY_RUNNER, groups = ["project-webrtc-tryjob-access"]),
    ],
    retry_config = cq.RETRY_ALL_FAILURES,
    cancel_stale_tryjobs = True,
)

luci.cq_group(
    name = "cq_infra",
    watch = [cq.refset(repo = WEBRTC_GERRIT, refs = ["refs/heads/infra/config"])],
    acls = [
        acl.entry(acl.CQ_COMMITTER, groups = ["project-webrtc-admins"]),
        acl.entry(acl.CQ_DRY_RUNNER, groups = ["project-webrtc-tryjob-access"]),
    ],
    retry_config = cq.RETRY_ALL_FAILURES,
    cancel_stale_tryjobs = True,
)

luci.cq_tryjob_verifier(
    builder = "presubmit",
    cq_group = "cq_infra",
)

# Internal-only tryjob always included into CQ:
luci.cq_tryjob_verifier(
    builder = "webrtc-internal:g3.webrtc-internal.try/internal_compile_lite",
    owner_whitelist = ["project-webrtc-internal-tryjob-access"],
    cq_group = "cq",
)

# Includable via `Cq-Include-Trybots: webrtc-internal/g3.webrtc-internal.try:internal_compile`:
luci.cq_tryjob_verifier(
    builder = "webrtc-internal:g3.webrtc-internal.try/internal_compile",
    owner_whitelist = ["project-webrtc-internal-tryjob-access"],
    cq_group = "cq",
    includable_only = True,
)

# Includable via `Cq-Include-Trybots: webrtc-internal/g3.webrtc-internal.try:internal_tests`:
luci.cq_tryjob_verifier(
    builder = "webrtc-internal:g3.webrtc-internal.try/internal_tests",
    owner_whitelist = ["project-webrtc-internal-tryjob-access"],
    cq_group = "cq",
    includable_only = True,
)

# Notifier definitions:

luci.notifier(
    name = "post_submit_failure_notifier",
    on_new_status = ["FAILURE"],
    notify_emails = [WEBRTC_TROOPER_EMAIL],
    notify_blamelist = True,
    template = luci.notifier_template(
        name = "build_failure",
        body = io.read_file("templates/build_failure.template"),
    ),
)

luci.notifier(
    name = "cron_notifier",
    on_new_status = ["FAILURE", "INFRA_FAILURE"],
    notify_emails = [WEBRTC_TROOPER_EMAIL],
    template = luci.notifier_template(
        name = "cron",
        body = io.read_file("templates/cron.template"),
    ),
)

luci.notifier(
    name = "infra_failure_notifier",
    on_new_status = ["INFRA_FAILURE"],
    notify_emails = [WEBRTC_TROOPER_EMAIL],
    template = luci.notifier_template(
        name = "infra_failure",
        body = io.read_file("templates/infra_failure.template"),
    ),
)

# Notify findit about completed builds for code coverage purposes
luci.buildbucket_notification_topic(
    name = "projects/findit-for-me/topics/buildbucket_notification",
)

# Tree closer definitions:

luci.tree_closer(
    name = "webrtc_tree_closer",
    tree_status_host = "webrtc-status.appspot.com",
    # TODO: These step filters are copied verbatim from Gatekeeper, for testing
    # that LUCI-Notify would take the exact same actions. Once we've switched
    # over, this should be updated - several of these steps don't exist in
    # WebRTC recipes.
    failed_step_regexp = [
        "bot_update",
        "compile",
        "gclient runhooks",
        "runhooks",
        "update",
        "extract build",
        "cleanup_temp",
        "taskkill",
        "compile",
        "gn",
    ],
    failed_step_regexp_exclude = ".*\\(experimental\\).*",
)

# Recipe definitions:

def recipe(recipe):
    return luci.recipe(
        name = recipe.split("/")[-1],
        cipd_package = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
        cipd_version = "refs/heads/main",
        recipe = recipe,
    )

recipe("chromium_trybot")
recipe("run_presubmit")
recipe("webrtc/auto_roll_webrtc_deps")
recipe("webrtc/ios_api_framework")
recipe("webrtc/libfuzzer")
recipe("webrtc/standalone")
recipe("webrtc/update_webrtc_binary_version")
recipe("lkgr_finder")
register_recipe_experiments("standalone", {})

# Console definitions:

luci.console_view(name = "ci", title = "Main", repo = WEBRTC_GIT, header = "console-header.textpb", refs = ["refs/heads/main"])
luci.console_view(name = "perf", title = "Perf", repo = WEBRTC_GIT, header = "console-header.textpb", refs = ["refs/heads/main"])
luci.list_view(name = "cron", title = "Cron")
luci.list_view(name = "try", title = "Tryserver")

exec("//builders.star")
