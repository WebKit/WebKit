#!/usr/bin/env lucicfg
# https://chromium.googlesource.com/infra/luci/luci-go/+/master/lucicfg/doc/

"""LUCI project configuration for libyuv CQ and CI."""

lucicfg.check_version("1.30.9")

LIBYUV_GIT = "https://chromium.googlesource.com/libyuv/libyuv"
LIBYUV_GERRIT = "https://chromium-review.googlesource.com/libyuv/libyuv"

RECLIENT_CI = {
    "instance": "rbe-webrtc-trusted",
    "metrics_project": "chromium-reclient-metrics",
}

RECLIENT_CQ = {
    "instance": "rbe-webrtc-untrusted",
    "metrics_project": "chromium-reclient-metrics",
}

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

luci.builder.defaults.experiments.set(
    {
        "luci.recipes.use_python3": 100,
    },
)

lucicfg.config(
    lint_checks = ["default"],
    config_dir = ".",
    tracked_files = [
        "commit-queue.cfg",
        "cr-buildbucket.cfg",
        "luci-logdog.cfg",
        "luci-milo.cfg",
        "luci-scheduler.cfg",
        "project.cfg",
        "realms.cfg",
    ],
)

# Generates project.cfg

luci.project(
    name = "libyuv",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(acl.PROJECT_CONFIGS_READER, groups = ["all"]),
        acl.entry(acl.LOGDOG_READER, groups = ["all"]),
        acl.entry(acl.LOGDOG_WRITER, groups = ["luci-logdog-chromium-writers"]),
        acl.entry(acl.SCHEDULER_READER, groups = ["all"]),
        acl.entry(acl.SCHEDULER_OWNER, groups = ["project-libyuv-admins"]),
        acl.entry(acl.BUILDBUCKET_READER, groups = ["all"]),
        acl.entry(acl.BUILDBUCKET_OWNER, groups = ["project-libyuv-admins"]),
    ],
    bindings = [
        luci.binding(
            roles = "role/swarming.taskTriggerer",  # for LED tasks.
            groups = "project-libyuv-admins",
        ),
        luci.binding(
            roles = "role/configs.validator",
            users = "libyuv-try-builder@chops-service-accounts.iam.gserviceaccount.com",
        ),
    ],
)

# Generates luci-logdog.cfg

luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

# Generates luci-scheduler.cfg

luci.gitiles_poller(
    name = "master-gitiles-trigger",
    bucket = "ci",
    repo = LIBYUV_GIT,
)

# Generates luci-milo.cfg

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra-public/logo/libyuv-logo.png",
)

def libyuv_ci_view(name, category, short_name):
    return luci.console_view_entry(
        console_view = "main",
        builder = name,
        category = category,
        short_name = short_name,
    )

def libyuv_try_view(name):
    return luci.list_view_entry(
        list_view = "try",
        builder = name,
    )

luci.console_view(
    name = "main",
    title = "libyuv Main Console",
    include_experimental_builds = True,
    repo = LIBYUV_GIT,
)

luci.list_view(
    name = "cron",
    title = "Cron",
    entries = ["DEPS Autoroller"],
)

luci.list_view(
    name = "try",
    title = "libyuv Try Builders",
)

# Generates commit-queue.cfg

def libyuv_try_job_verifier(name, cq_group, experiment_percentage):
    return luci.cq_tryjob_verifier(
        builder = name,
        cq_group = cq_group,
        experiment_percentage = experiment_percentage,
    )

luci.cq(
    status_host = "chromium-cq-status.appspot.com",
    submit_max_burst = 4,
    submit_burst_delay = 8 * time.minute,
)

luci.cq_group(
    name = "master",
    watch = [
        cq.refset(
            repo = LIBYUV_GERRIT,
            refs = ["refs/heads/main", "refs/heads/master"],
        ),
    ],
    acls = [
        acl.entry(acl.CQ_COMMITTER, groups = ["project-libyuv-committers"]),
        acl.entry(acl.CQ_DRY_RUNNER, groups = ["project-libyuv-tryjob-access"]),
    ],
    retry_config = cq.RETRY_ALL_FAILURES,
    cancel_stale_tryjobs = True,
)

luci.cq_group(
    name = "config",
    watch = [
        cq.refset(
            repo = LIBYUV_GERRIT,
            refs = ["refs/heads/infra/config"],
        ),
    ],
    acls = [
        acl.entry(acl.CQ_COMMITTER, groups = ["project-libyuv-committers"]),
        acl.entry(acl.CQ_DRY_RUNNER, groups = ["project-libyuv-tryjob-access"]),
    ],
    retry_config = cq.RETRY_ALL_FAILURES,
    cancel_stale_tryjobs = True,
)

# Generates cr-buildbucket.cfg

luci.bucket(
    name = "ci",
)
luci.bucket(
    name = "try",
    acls = [
        acl.entry(acl.BUILDBUCKET_TRIGGERER, groups = [
            "project-libyuv-tryjob-access",
            "service-account-cq",
        ]),
    ],
)
luci.bucket(
    name = "cron",
)

def get_os_dimensions(os):
    if os == "android":
        return {"device_type": "walleye"}
    if os == "ios" or os == "mac":
        return {"os": "Mac-12", "cpu": "x86-64"}
    elif os == "win":
        return {"os": "Windows-10", "cores": "8", "cpu": "x86-64"}
    elif os == "linux":
        return {"os": "Ubuntu-22.04", "cores": "8", "cpu": "x86-64"}
    return {}

def libyuv_ci_builder(name, dimensions, properties, triggered_by):
    return luci.builder(
        name = name,
        dimensions = dimensions,
        properties = properties,
        bucket = "ci",
        service_account = "libyuv-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        triggered_by = triggered_by,
        swarming_tags = ["vpython:native-python-wrapper"],
        execution_timeout = 180 * time.minute,
        build_numbers = True,
        executable = luci.recipe(
            name = "libyuv/libyuv",
            cipd_package = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
            use_python3 = True,
        ),
    )

def libyuv_try_builder(name, dimensions, properties, recipe_name = "libyuv/libyuv"):
    return luci.builder(
        name = name,
        dimensions = dimensions,
        properties = properties,
        bucket = "try",
        service_account = "libyuv-try-builder@chops-service-accounts.iam.gserviceaccount.com",
        swarming_tags = ["vpython:native-python-wrapper"],
        execution_timeout = 180 * time.minute,
        build_numbers = True,
        executable = luci.recipe(
            name = recipe_name,
            cipd_package = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
            use_python3 = True,
        ),
    )

def ci_builder(name, os, category, short_name = None):
    dimensions = get_os_dimensions(os)
    properties = {"$build/reclient": RECLIENT_CI}

    dimensions["pool"] = "luci.flex.ci"
    properties["builder_group"] = "client.libyuv"

    triggered_by = ["master-gitiles-trigger" if os != "android" else "Android Debug"]
    libyuv_ci_view(name, category, short_name)
    return libyuv_ci_builder(name, dimensions, properties, triggered_by)

def try_builder(name, os, experiment_percentage = None):
    dimensions = get_os_dimensions(os)
    properties = {"$build/reclient": RECLIENT_CQ}

    dimensions["pool"] = "luci.flex.try"
    properties["builder_group"] = "tryserver.libyuv"

    if name == "presubmit":
        recipe_name = "run_presubmit"
        properties["repo_name"] = "libyuv"
        properties["runhooks"] = True
        libyuv_try_job_verifier(name, "config", experiment_percentage)
        return libyuv_try_builder(name, dimensions, properties, recipe_name)

    libyuv_try_job_verifier(name, "master", experiment_percentage)
    libyuv_try_view(name)
    return libyuv_try_builder(name, dimensions, properties)

luci.builder(
    name = "DEPS Autoroller",
    bucket = "cron",
    service_account = "libyuv-ci-autoroll-builder@chops-service-accounts.iam.gserviceaccount.com",
    dimensions = {
        "pool": "luci.webrtc.cron",
        "os": "Linux",
        "cpu": "x86-64",
    },
    swarming_tags = ["vpython:native-python-wrapper"],
    execution_timeout = 120 * time.minute,
    build_numbers = True,
    schedule = "0 14 * * *",  # Every 2 hours.
    executable = luci.recipe(
        name = "libyuv/roll_deps",
        cipd_package = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
        use_python3 = True,
    ),
)

ci_builder("Android ARM64 Debug", "linux", "Android|Builder", "dbg")
ci_builder("Android Debug", "linux", "Android|Builder", "dbg")
ci_builder("Android Release", "linux", "Android|Builder", "rel")
ci_builder("Android32 x86 Debug", "linux", "Android|Builder|x86", "dbg")
ci_builder("Android64 x64 Debug", "linux", "Android|Builder|x64", "dbg")
ci_builder("Android Tester ARM32 Debug (Nexus 5X)", "android", "Android|Tester|ARM 32", "dbg")
ci_builder("Android Tester ARM32 Release (Nexus 5X)", "android", "Android|Tester|ARM 32", "rel")
ci_builder("Android Tester ARM64 Debug (Nexus 5X)", "android", "Android|Tester|ARM 64", "dbg")
ci_builder("Linux Asan", "linux", "Linux", "asan")
ci_builder("Linux MSan", "linux", "Linux", "msan")
ci_builder("Linux Tsan v2", "linux", "Linux", "tsan")
ci_builder("Linux UBSan", "linux", "Linux|UBSan")
ci_builder("Linux UBSan vptr", "linux", "Linux|UBSan", "vptr")
ci_builder("Linux32 Debug", "linux", "Linux|32", "dbg")
ci_builder("Linux32 Release", "linux", "Linux|32", "rel")
ci_builder("Linux64 Debug", "linux", "Linux|64", "dbg")
ci_builder("Linux64 Release", "linux", "Linux|64", "rel")
ci_builder("Mac Asan", "mac", "Mac", "asan")
ci_builder("Mac64 Debug", "mac", "Mac", "dbg")
ci_builder("Mac64 Release", "mac", "Mac", "rel")
ci_builder("Win32 Debug", "win", "Win|32|Debug")
ci_builder("Win32 Debug (Clang)", "win", "Win|32|Debug", "clg")
ci_builder("Win32 Release", "win", "Win|32|Release")
ci_builder("Win32 Release (Clang)", "win", "Win|32|Release", "clg")
ci_builder("Win64 Debug", "win", "Win|64|Debug", "clg")
ci_builder("Win64 Debug (Clang)", "win", "Win|64|Debug", "clg")
ci_builder("Win64 Release", "win", "Win|64|Release")
ci_builder("Win64 Release (Clang)", "win", "Win|64|Release", "clg")
ci_builder("iOS ARM64 Debug", "ios", "iOS|ARM64", "dbg")
ci_builder("iOS ARM64 Release", "ios", "iOS|ARM64", "rel")

# TODO(crbug.com/1242847): make this not experimental.
try_builder("android", "android", experiment_percentage = 100)
try_builder("android_arm64", "android", experiment_percentage = 100)
try_builder("android_rel", "android", experiment_percentage = 100)

try_builder("android_x64", "linux")
try_builder("android_x86", "linux")
try_builder("ios_arm64", "ios")
try_builder("ios_arm64_rel", "ios")
try_builder("linux", "linux")
try_builder("linux_asan", "linux")
try_builder("linux_gcc", "linux", experiment_percentage = 100)
try_builder("linux_msan", "linux")
try_builder("linux_rel", "linux")
try_builder("linux_tsan2", "linux")
try_builder("linux_ubsan", "linux")
try_builder("linux_ubsan_vptr", "linux")
try_builder("mac", "mac")
try_builder("mac_asan", "mac")
try_builder("mac_rel", "mac")
try_builder("win", "win")
try_builder("win_clang", "win")
try_builder("win_clang_rel", "win")
try_builder("win_rel", "win")
try_builder("win_x64_clang_rel", "win")
try_builder("win_x64_rel", "win")
try_builder("presubmit", "linux")
