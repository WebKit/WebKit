#!/usr/bin/env lucicfg
#
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# main.star: lucicfg configuration for ANGLE's standalone builders.

lucicfg.check_version(min = "1.31.3", message = "Update depot_tools")

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

lucicfg.config(
    fail_on_warnings = True,
    lint_checks = [
        "default",
        "-module-docstring",
        "-function-docstring",
    ],
)

luci.project(
    name = "angle",
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

# The category for an os: a more generic grouping than specific OS versions that
# can be used for computing defaults
os_category = struct(
    ANDROID = "Android",
    LINUX = "Linux",
    MAC = "Mac",
    WINDOWS = "Windows",
)

def os_enum(dimension, category, console_name):
    return struct(dimension = dimension, category = category, console_name = console_name)

os = struct(
    ANDROID = os_enum("Ubuntu", os_category.ANDROID, "android"),
    LINUX = os_enum("Ubuntu", os_category.LINUX, "linux"),
    MAC = os_enum("Mac", os_category.MAC, "mac"),
    WINDOWS = os_enum("Windows", os_category.WINDOWS, "win"),
)

# Recipes

_RECIPE_NAME_PREFIX = "recipe:"
_DEFAULT_BUILDERLESS_OS_CATEGORIES = [os_category.LINUX, os_category.WINDOWS]
_GOMA_RBE_PROD = {
    "server_host": "goma.chromium.org",
    "rpc_extra_params": "?prod",
    "use_luci_auth": True,
}

def _recipe_for_package(cipd_package):
    def recipe(*, name, cipd_version = None, recipe = None, use_python3 = False):
        # Force the caller to put the recipe prefix rather than adding it
        # programatically to make the string greppable
        if not name.startswith(_RECIPE_NAME_PREFIX):
            fail("Recipe name {!r} does not start with {!r}"
                .format(name, _RECIPE_NAME_PREFIX))
        if recipe == None:
            recipe = name[len(_RECIPE_NAME_PREFIX):]
        return luci.recipe(
            name = name,
            cipd_package = cipd_package,
            cipd_version = cipd_version,
            recipe = recipe,
            use_bbagent = True,
            use_python3 = use_python3,
        )

    return recipe

build_recipe = _recipe_for_package(
    "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
)

build_recipe(
    name = "recipe:angle",
    use_python3 = True,
)

build_recipe(
    name = "recipe:run_presubmit",
    use_python3 = True,
)

def get_os_from_name(name):
    if name.startswith("android"):
        return os.ANDROID
    if name.startswith("linux"):
        return os.LINUX
    if name.startswith("win"):
        return os.WINDOWS
    if name.startswith("mac"):
        return os.MAC
    return os.MAC

def get_gpu_type_from_builder_name(name):
    return name.split("-")[1]

# Adds both the CI and Try standalone builders.
def angle_builder(name, cpu):
    config_os = get_os_from_name(name)
    dimensions = {}
    dimensions["os"] = config_os.dimension

    goma_props = {}
    goma_props.update(_GOMA_RBE_PROD)

    if config_os.category in _DEFAULT_BUILDERLESS_OS_CATEGORIES:
        dimensions["builderless"] = "1"
        goma_props["enable_ats"] = True

    is_asan = "-asan" in name
    is_tsan = "-tsan" in name
    is_ubsan = "-ubsan" in name
    is_debug = "-dbg" in name
    is_exp = "-exp" in name
    is_perf = name.endswith("-perf")
    is_trace = name.endswith("-trace")
    is_uwp = "winuwp" in name
    is_msvc = is_uwp or "-msvc" in name

    location_filters = None

    if name.endswith("-compile"):
        test_mode = "compile_only"
        category = "compile"
    elif name.endswith("-test"):
        test_mode = "compile_and_test"
        category = "test"
    elif is_trace:
        test_mode = "trace_tests"
        category = "trace"

        # Trace tests are only run on CQ if files in the capture folders change.
        location_filters = [
            cq.location_filter(path_regexp = "DEPS"),
            cq.location_filter(path_regexp = "src/libANGLE/capture/.+"),
            cq.location_filter(path_regexp = "src/tests/angle_end2end_tests_expectations.txt"),
            cq.location_filter(path_regexp = "src/tests/capture.+"),
            cq.location_filter(path_regexp = "src/tests/egl_tests/.+"),
            cq.location_filter(path_regexp = "src/tests/gl_tests/.+"),
        ]
    elif is_perf:
        test_mode = "compile_and_test"
        category = "perf"
    else:
        print("Test mode unknown for %s" % name)

    if is_msvc:
        toolchain = "msvc"
    else:
        toolchain = "clang"

    if is_uwp:
        os_name = "winuwp"
    else:
        os_name = config_os.console_name

    if is_perf:
        short_name = get_gpu_type_from_builder_name(name)
    elif is_asan:
        short_name = "asan"
    elif is_tsan:
        short_name = "tsan"
    elif is_ubsan:
        short_name = "ubsan"
    elif is_debug:
        short_name = "dbg"
    elif is_exp:
        short_name = "exp"
    else:
        short_name = "rel"

    properties = {
        "builder_group": "angle",
        "$build/goma": goma_props,
        "platform": config_os.console_name,
        "toolchain": toolchain,
        "test_mode": test_mode,
    }

    ci_properties = {
        "builder_group": "angle",
        "$build/goma": goma_props,
        "platform": config_os.console_name,
        "toolchain": toolchain,
        "test_mode": test_mode,
    }

    ci_properties["sheriff_rotations"] = ["angle"]

    if is_perf:
        timeout_hours = 5
    else:
        timeout_hours = 3

    luci.builder(
        name = name,
        bucket = "ci",
        triggered_by = ["main-poller"],
        executable = "recipe:angle",
        service_account = "angle-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        properties = ci_properties,
        dimensions = dimensions,
        build_numbers = True,
        resultdb_settings = resultdb.settings(enable = True),
        test_presentation = resultdb.test_presentation(
            column_keys = ["v.gpu"],
            grouping_keys = ["status", "v.test_suite"],
        ),
        triggering_policy = scheduler.policy(
            kind = scheduler.LOGARITHMIC_BATCHING_KIND,
            log_base = 2,
        ),
        execution_timeout = timeout_hours * time.hour,
    )

    luci.console_view_entry(
        console_view = "ci",
        builder = "ci/" + name,
        category = category + "|" + os_name + "|" + toolchain + "|" + cpu,
        short_name = short_name,
    )

    # Do not include perf tests in "try".
    # TSAN is also excluded from "try" due to excessive flakiness: crbug.com/1275223
    if not is_perf and not is_tsan:
        luci.list_view_entry(
            list_view = "try",
            builder = "try/" + name,
        )

        luci.builder(
            name = name,
            bucket = "try",
            executable = "recipe:angle",
            service_account = "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            properties = properties,
            dimensions = dimensions,
            build_numbers = True,
            resultdb_settings = resultdb.settings(enable = True),
            test_presentation = resultdb.test_presentation(
                column_keys = ["v.gpu"],
                grouping_keys = ["status", "v.test_suite"],
            ),
        )

        # Don't add experimental bots to CQ.
        if not is_exp:
            luci.cq_tryjob_verifier(
                cq_group = "main",
                builder = "angle:try/" + name,
                location_filters = location_filters,
            )

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

luci.builder(
    name = "presubmit",
    bucket = "try",
    executable = "recipe:run_presubmit",
    service_account = "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
    build_numbers = True,
    dimensions = {
        "os": os.LINUX.dimension,
    },
    properties = {
        "repo_name": "angle",
        "runhooks": True,
    },
    resultdb_settings = resultdb.settings(enable = True),
    test_presentation = resultdb.test_presentation(
        column_keys = ["v.gpu"],
        grouping_keys = ["status", "v.test_suite"],
    ),
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

# name, clang, debug, cpu, uwp, trace_tests
angle_builder("android-arm-compile", cpu = "arm")
angle_builder("android-arm-dbg-compile", cpu = "arm")
angle_builder("android-arm64-dbg-compile", cpu = "arm64")
angle_builder("android-arm64-exp-test", cpu = "arm64")
angle_builder("android-arm64-test", cpu = "arm64")
angle_builder("linux-asan-test", cpu = "x64")
angle_builder("linux-exp-test", cpu = "x64")
angle_builder("linux-tsan-test", cpu = "x64")
angle_builder("linux-ubsan-test", cpu = "x64")
angle_builder("linux-dbg-compile", cpu = "x64")
angle_builder("linux-test", cpu = "x64")
angle_builder("mac-dbg-compile", cpu = "x64")
angle_builder("mac-exp-test", cpu = "x64")
angle_builder("mac-test", cpu = "x64")
angle_builder("win-asan-test", cpu = "x64")
angle_builder("win-dbg-compile", cpu = "x64")
angle_builder("win-msvc-compile", cpu = "x64")
angle_builder("win-msvc-dbg-compile", cpu = "x64")
angle_builder("win-msvc-x86-compile", cpu = "x86")
angle_builder("win-msvc-x86-dbg-compile", cpu = "x86")
angle_builder("win-test", cpu = "x64")
angle_builder("win-x86-dbg-compile", cpu = "x86")
angle_builder("win-x86-test", cpu = "x86")
angle_builder("winuwp-compile", cpu = "x64")
angle_builder("winuwp-dbg-compile", cpu = "x64")

angle_builder("linux-trace", cpu = "x64")
angle_builder("win-trace", cpu = "x64")

angle_builder("android-pixel4-perf", cpu = "arm64")
angle_builder("linux-intel-uhd630-perf", cpu = "x64")
angle_builder("linux-nvidia-gtx1660-perf", cpu = "x64")
angle_builder("win10-intel-uhd630-perf", cpu = "x64")
angle_builder("win10-nvidia-gtx1660-perf", cpu = "x64")

# Views

luci.console_view(
    name = "ci",
    title = "ANGLE CI Builders",
    repo = "https://chromium.googlesource.com/angle/angle",
)

luci.list_view(
    name = "try",
    title = "ANGLE Try Builders",
)

luci.list_view_entry(
    list_view = "try",
    builder = "try/presubmit",
)

# CQ

luci.cq(
    status_host = "chromium-cq-status.appspot.com",
    submit_max_burst = 4,
    submit_burst_delay = 480 * time.second,
)

luci.cq_group(
    name = "main",
    watch = cq.refset(
        "https://chromium.googlesource.com/angle/angle",
        refs = [r"refs/heads/main"],
    ),
    acls = [
        acl.entry(
            acl.CQ_COMMITTER,
            groups = "project-angle-committers",
        ),
        acl.entry(
            acl.CQ_DRY_RUNNER,
            groups = "project-angle-tryjob-access",
        ),
    ],
    verifiers = [
        luci.cq_tryjob_verifier(
            builder = "angle:try/presubmit",
            disable_reuse = True,
        ),
        luci.cq_tryjob_verifier(
            builder = "chromium:try/android-angle-chromium-try",
        ),
        luci.cq_tryjob_verifier(
            builder = "chromium:try/fuchsia-angle-try",
        ),
        luci.cq_tryjob_verifier(
            builder = "chromium:try/linux-angle-chromium-try",
        ),
        luci.cq_tryjob_verifier(
            builder = "chromium:try/mac-angle-chromium-try",
        ),
        luci.cq_tryjob_verifier(
            builder = "chromium:try/win-angle-chromium-x64-try",
        ),
        luci.cq_tryjob_verifier(
            builder = "chromium:try/win-angle-chromium-x86-try",
        ),
    ],
)
