# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Legacy builder definitions that were originally in main.star

Moved due to them causing issues while working on anglebug.com/475260235. Any
builders that will be kept after that migration should be grouped and moved
into appropriately named files.
"""

# Fail build when merge script fails.
build_experiments = {"chromium_swarming.expose_merge_script_failures": 100}

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

_DEFAULT_BUILDERLESS_OS_CATEGORIES = [os_category.LINUX, os_category.WINDOWS, os_category.MAC]
_CHROMIUM_POOL_OS_CATEGORIES = [os_category.MAC]

def get_os_from_name(name):
    """Returns os enum given the name.

    Args:
        name: A name to check against.

    Returns:
        An os enum given the name.
    """
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
    """Adds a CI and Try standalone builder.

    Args:
        name: string representing name of the builder.
        cpu: string representing CPU archiecture of builder.
    """
    config_os = get_os_from_name(name)
    dimensions = {}
    dimensions["os"] = config_os.dimension

    if config_os.category in _DEFAULT_BUILDERLESS_OS_CATEGORIES:
        dimensions["builderless"] = "1"

    ci_dimensions = {}
    try_dimensions = {}
    ci_dimensions.update(dimensions)
    try_dimensions.update(dimensions)

    # TODO(crbug.com/375244064): Make the Chromium pools the default everywhere
    # once all pool capacity is merged.
    migrated_to_chromium_pool = config_os.category in _CHROMIUM_POOL_OS_CATEGORIES
    if migrated_to_chromium_pool:
        ci_dimensions["pool"] = "luci.chromium.gpu.ci"
        try_dimensions["pool"] = "luci.chromium.gpu.try"

    is_asan = "-asan" in name
    is_tsan = "-tsan" in name
    is_debug = "-dbg" in name
    is_exp = "-exp" in name
    is_ir = "-ir" in name
    is_perf = name.endswith("-perf")
    is_pixel10 = "pixel10" in name
    is_s24 = "s24" in name
    is_trace = name.endswith("-trace")
    is_uwp = "winuwp" in name
    is_msvc = is_uwp or "-msvc" in name

    location_filters = None

    test_mode = ""
    category = ""
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
        os_toolchain_name = "win-uwp"
    elif is_msvc:
        os_toolchain_name = "win-msvc"
    else:
        os_toolchain_name = config_os.console_name

    if is_perf:
        short_name = get_gpu_type_from_builder_name(name)
    elif is_asan:
        short_name = "asan"
        if is_exp:
            short_name = "asan-exp"
    elif is_tsan:
        short_name = "tsan"
        if is_exp:
            short_name = "tsan-exp"
    elif is_debug:
        short_name = "dbg"
    elif is_exp:
        short_name = "exp"
        if is_s24:
            # This is a little clunky, but we'd like this to be cleanly "s24" rather than "s24-exp"
            short_name = "s24"
        elif is_pixel10:
            short_name = "p10"
    elif is_ir:
        short_name = "ir"
    else:
        short_name = "rel"

    properties = {
        "$build/siso": {
            "configs": ["builder"],
            "enable_cloud_monitoring": True,
            "enable_cloud_profiler": True,
            "enable_cloud_trace": True,
            "project": "rbe-chromium-untrusted",
        },
        "builder_group": "angle",
        "platform": config_os.console_name,
        "test_mode": test_mode,
        "toolchain": toolchain,
    }

    ci_properties = {
        "$build/siso": {
            "configs": ["builder"],
            "enable_cloud_monitoring": True,
            "enable_cloud_profiler": True,
            "enable_cloud_trace": True,
            "project": "rbe-chromium-trusted",
        },
        "builder_group": "angle",
        "platform": config_os.console_name,
        "test_mode": test_mode,
        "toolchain": toolchain,
    }

    # TODO(343503161): Remove sheriff_rotations after SoM is updated.
    ci_properties["gardener_rotations"] = ["angle"]
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
        experiments = build_experiments,
        service_account = "angle-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        shadow_service_account = "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
        properties = ci_properties,
        dimensions = ci_dimensions,
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

    active_experimental_builders = [
        "android-arm64-exp-test",
        "android-arm64-exp-pixel10-test",
        "android-arm64-exp-s24-test",
        "mac-exp-test",  # temporarily used for AMD Radeon Pro 555X
        "win-exp-test",  # temporarily used for Intel UHD770
    ]

    if (not is_exp and not is_ir) or (name in active_experimental_builders):
        luci.console_view_entry(
            console_view = "ci",
            builder = "ci/" + name,
            category = category + "|" + os_toolchain_name + "|" + cpu,
            short_name = short_name,
        )
    else:
        luci.list_view_entry(
            list_view = "exp",
            builder = "ci/" + name,
        )

    # Do not include perf tests in "try".
    if not is_perf:
        luci.list_view_entry(
            list_view = "try",
            builder = "try/" + name,
        )

        max_concurrent_builds = None

        # Don't add experimental bots to CQ.
        # IR bots are also experimental currently.
        add_to_cq = (not is_exp and not is_ir)
        if migrated_to_chromium_pool:
            if add_to_cq:
                max_concurrent_builds = 5
            else:
                max_concurrent_builds = 1

        luci.builder(
            name = name,
            bucket = "try",
            executable = "recipe:angle",
            experiments = build_experiments,
            service_account = "angle-try-builder@chops-service-accounts.iam.gserviceaccount.com",
            properties = properties,
            dimensions = try_dimensions,
            build_numbers = True,
            resultdb_settings = resultdb.settings(enable = True),
            test_presentation = resultdb.test_presentation(
                column_keys = ["v.gpu"],
                grouping_keys = ["status", "v.test_suite"],
            ),
            max_concurrent_builds = max_concurrent_builds,
        )

        # Don't add experimental bots to CQ.
        if add_to_cq:
            luci.cq_tryjob_verifier(
                cq_group = "main",
                builder = "angle:try/" + name,
                location_filters = location_filters,
            )

luci.builder(
    name = "presubmit",
    bucket = "try",
    executable = "recipe:run_presubmit",
    experiments = build_experiments,
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

# name, clang, debug, cpu, uwp, trace_tests
angle_builder("android-arm-compile", cpu = "arm")
angle_builder("android-arm-dbg-compile", cpu = "arm")
angle_builder("android-arm64-dbg-compile", cpu = "arm64")
angle_builder("android-arm64-exp-pixel10-test", cpu = "arm64")
angle_builder("android-arm64-exp-s24-test", cpu = "arm64")
angle_builder("android-arm64-exp-test", cpu = "arm64")
angle_builder("android-arm64-ir-test", cpu = "arm64")
angle_builder("android-arm64-test", cpu = "arm64")
angle_builder("linux-asan-test", cpu = "x64")
angle_builder("linux-exp-asan-test", cpu = "x64")
angle_builder("linux-exp-test", cpu = "x64")
angle_builder("linux-exp-tsan-test", cpu = "x64")
angle_builder("linux-tsan-test", cpu = "x64")
angle_builder("linux-dbg-compile", cpu = "x64")
angle_builder("linux-ir-test", cpu = "x64")
angle_builder("linux-test", cpu = "x64")
angle_builder("mac-dbg-compile", cpu = "x64")
angle_builder("mac-arm64-test", cpu = "arm64")
angle_builder("mac-exp-test", cpu = "x64")
angle_builder("mac-ir-test", cpu = "x64")
angle_builder("mac-test", cpu = "x64")
angle_builder("win-asan-test", cpu = "x64")
angle_builder("win-dbg-compile", cpu = "x64")
angle_builder("win-exp-test", cpu = "x64")
angle_builder("win-msvc-compile", cpu = "x64")
angle_builder("win-msvc-dbg-compile", cpu = "x64")
angle_builder("win-msvc-x86-compile", cpu = "x86")
angle_builder("win-msvc-x86-dbg-compile", cpu = "x86")
angle_builder("win-ir-test", cpu = "x64")
angle_builder("win-test", cpu = "x64")
angle_builder("win-x86-dbg-compile", cpu = "x86")
angle_builder("win-x86-test", cpu = "x86")
angle_builder("winuwp-compile", cpu = "x64")
angle_builder("winuwp-dbg-compile", cpu = "x64")

angle_builder("linux-trace", cpu = "x64")
angle_builder("win-trace", cpu = "x64")

angle_builder("android-pixel4-perf", cpu = "arm64")
angle_builder("android-pixel6-perf", cpu = "arm64")
angle_builder("linux-intel-uhd630-perf", cpu = "x64")
angle_builder("linux-nvidia-gtx1660-perf", cpu = "x64")
angle_builder("win10-intel-uhd630-perf", cpu = "x64")
angle_builder("win10-nvidia-gtx1660-perf", cpu = "x64")

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
            groups = "project-angle-submit-access",
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
