#!/usr/bin/env lucicfg

"""
lucicfg definitions for BoringSSL's CI and CQ.
"""

lucicfg.check_version("1.30.9")

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

lucicfg.config(
    lint_checks = ["default"],
)

REPO_URL = "https://boringssl.googlesource.com/boringssl"

# The default recipe is "boringssl.py"
RECIPE_BUNDLE = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build"

luci.project(
    name = "boringssl",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.BUILDBUCKET_READER,
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = acl.CQ_COMMITTER,
            groups = "project-boringssl-committers",
        ),
        acl.entry(
            roles = acl.CQ_DRY_RUNNER,
            groups = "project-boringssl-tryjob-access",
        ),
        acl.entry(
            roles = acl.CQ_NEW_PATCHSET_RUN_TRIGGERER,
            groups = "project-boringssl-tryjob-access",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-boringssl-admins",
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = "luci-logdog-chromium-writers",
        ),
    ],
)

luci.bucket(name = "ci")

luci.bucket(
    name = "try",
    acls = [
        # Allow launching tryjobs directly (in addition to doing it through CQ).
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = [
                "project-boringssl-tryjob-access",
                "service-account-cq",
            ],
        ),
    ],
)

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra/boringssl-logo.png",
)

console_view = luci.console_view(
    name = "main",
    repo = REPO_URL,
    refs = ["refs/heads/main"],
    title = "BoringSSL Main Console",
)

luci.cq(
    submit_max_burst = 4,
    submit_burst_delay = 480 * time.second,
    # TODO(davidben): Can this be removed? It is marked as optional and
    # deprecated. It was included as part of porting over from commit-queue.cfg.
    status_host = "chromium-cq-status.appspot.com",
    honor_gerrit_linked_accounts = True,
)

cq_group = luci.cq_group(
    name = "main-cq",
    watch = cq.refset(REPO_URL, refs = ["refs/heads/.+"]),
    retry_config = cq.RETRY_ALL_FAILURES,
)

poller = luci.gitiles_poller(
    name = "main-gitiles-trigger",
    bucket = "ci",
    repo = REPO_URL,
    refs = ["refs/heads/main"],
)

luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

notifier = luci.notifier(
    name = "all",
    on_occurrence = ["FAILURE", "INFRA_FAILURE"],
    on_new_status = ["SUCCESS"],
    notify_emails = ["boringssl@google.com"],
)

DEFAULT_TIMEOUT = 30 * time.minute

def ci_builder(
        name,
        host,
        *,
        recipe = "boringssl",
        category = None,
        short_name = None,
        execution_timeout = None,
        properties = {}):
    """Defines a CI builder.

    Args:
      name: The name to use for the builder.
      host: The host to run on.
      recipe: The recipe to run.
      category: Category in which to display the builder in the console view.
      short_name: The short name for the builder in the console view.
      execution_timeout: Overrides the default timeout.
      properties: Properties to pass to the recipe.
    """
    dimensions = dict(host["dimensions"])
    dimensions["pool"] = "luci.flex.ci"
    caches = [swarming.cache("gocache"), swarming.cache("gopath")]
    if "caches" in host:
        caches += host["caches"]
    properties = dict(properties)
    properties["$gatekeeper"] = {"group": "client.boringssl"}
    if execution_timeout == None:
        execution_timeout = host.get("execution_timeout", DEFAULT_TIMEOUT)
    builder = luci.builder(
        name = name,
        bucket = "ci",
        executable = luci.recipe(
            name = recipe,
            cipd_package = RECIPE_BUNDLE,
        ),
        service_account = "boringssl-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        dimensions = dimensions,
        execution_timeout = execution_timeout,
        caches = caches,
        notifies = [notifier],
        triggered_by = [poller],
        properties = properties,
    )
    luci.console_view_entry(
        builder = builder,
        console_view = console_view,
        category = category,
        short_name = short_name,
    )

def cq_builder(
        name,
        host,
        *,
        recipe = "boringssl",
        cq_enabled = True,
        execution_timeout = None,
        properties = {}):
    """Defines a CQ builder.

    Args:
      name: The name to use for the builder.
      host: The host to run on.
      recipe: The recipe to run.
      cq_enabled: Whether the try builder is enabled by default. (If false,
        the builder is includable_only.)
      execution_timeout: Overrides the default timeout.
      properties: Properties to pass to the recipe.
    """
    dimensions = dict(host["dimensions"])
    dimensions["pool"] = "luci.flex.try"
    if execution_timeout == None:
        execution_timeout = host.get("execution_timeout", DEFAULT_TIMEOUT)
    builder = luci.builder(
        name = name,
        bucket = "try",
        executable = luci.recipe(
            name = recipe,
            cipd_package = RECIPE_BUNDLE,
        ),
        service_account = "boringssl-try-builder@chops-service-accounts.iam.gserviceaccount.com",
        dimensions = dimensions,
        execution_timeout = host.get("execution_timeout", DEFAULT_TIMEOUT),
        caches = host.get("caches"),
        properties = properties,
    )
    luci.cq_tryjob_verifier(
        builder = builder,
        cq_group = cq_group,
        includable_only = not cq_enabled,
    )

luci.cq_tryjob_verifier(
    cq_group = "main-cq",
    builder = "chromium:try/tricium-clang-tidy",
    owner_whitelist = ["project-boringssl-tryjob-access"],
    experiment_percentage = 100,
    disable_reuse = True,
    mode_allowlist = [cq.MODE_NEW_PATCHSET_RUN],
    location_filters = [
        cq.location_filter(path_regexp = r".+\.h"),
        cq.location_filter(path_regexp = r".+\.c"),
        cq.location_filter(path_regexp = r".+\.cc"),
        cq.location_filter(path_regexp = r".+\.cpp"),
    ],
)

def both_builders(
        name,
        host,
        *,
        recipe = "boringssl",
        category = None,
        short_name = None,
        cq_enabled = True,
        cq_compile_only = None,
        execution_timeout = None,
        properties = {}):
    """Defines both a CI builder and similarly-configured CQ builder.

    Args:
      name: The name to use for both builders.
      host: The host to run on.
      recipe: The recipe to run.
      category: Category in which to display the builder in the console view.
      short_name: The short name for the builder in the console view.
      cq_enabled: Whether the try builder is enabled by default. (If false,
        the builder is includable_only.)
      cq_compile_only: If cq_compile_only is specified, we generate both a
        disabled builder that matches the CI builder, and a compile-only
        builder. The compile-only builder is controlled by cq_enabled.
        cq_compile_only also specifies the host to run on, because the
        compile-only builder usually has weaker requirements.
      execution_timeout: Overrides the default timeout.
      properties: Properties to pass to the recipe.
    """
    ci_builder(
        name,
        host,
        recipe = recipe,
        category = category,
        short_name = short_name,
        execution_timeout = execution_timeout,
        properties = properties,
    )

    cq_builder(
        name,
        host,
        recipe = recipe,
        cq_enabled = cq_enabled and not cq_compile_only,
        execution_timeout = execution_timeout,
        properties = properties,
    )
    if cq_compile_only:
        compile_properties = dict(properties)
        compile_properties["run_unit_tests"] = False
        compile_properties["run_ssl_tests"] = False
        cq_builder(
            name + "_compile",
            cq_compile_only,
            recipe = recipe,
            cq_enabled = cq_enabled,
            execution_timeout = execution_timeout,
            properties = compile_properties,
        )

LINUX_HOST = {
    "dimensions": {
        "os": "Ubuntu-22.04",
        "cpu": "x86-64",
    },
}

MAC_ARM64_HOST = {
    "dimensions": {
        "os": "Mac",
        "cpu": "arm64",
    },
    "caches": [swarming.cache("osx_sdk")],
    # xcode installation can take a while, particularly when running
    # concurrently on multiple VMs on the same host. See crbug.com/1063870
    # for more context.
    "execution_timeout": 60 * time.minute,
}

MAC_X86_64_HOST = {
    "dimensions": {
        # macOS 12 or later is needed as of Go 1.25.
        "os": "Mac-12|Mac-13",
        "cpu": "x86-64",
    },
    "caches": [swarming.cache("osx_sdk")],
    # xcode installation can take a while, particularly when running
    # concurrently on multiple VMs on the same host. See crbug.com/1063870
    # for more context.
    "execution_timeout": 60 * time.minute,
}

WIN_HOST = {
    "dimensions": {
        "os": "Windows-10",
        "cpu": "x86-64",
    },
    "caches": [swarming.cache("win_toolchain")],
}

# The Android tests take longer to run. See https://crbug.com/900953.
ANDROID_TIMEOUT = 60 * time.minute

WALLEYE_HOST = {
    "dimensions": {
        "device_type": "walleye",  # Pixel 2
    },
    "execution_timeout": ANDROID_TIMEOUT,
}

# SDE tests take longer to run.
SDE_TIMEOUT = 3 * 60 * time.minute

# TODO(davidben): Switch the BoringSSL recipe to specify most flags in
# properties rather than parsing names. Then we can add new configurations
# without having to touch multiple repositories.

both_builders(
    "android_aarch64",
    WALLEYE_HOST,
    category = "android|aarch64",
    short_name = "dbg",
    cq_compile_only = LINUX_HOST,
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "arm64-v8a",
            "ANDROID_PLATFORM": "android-21",
        },
    },
)
both_builders(
    "android_aarch64_rel",
    WALLEYE_HOST,
    category = "android|aarch64",
    short_name = "rel",
    cq_compile_only = LINUX_HOST,
    cq_enabled = False,
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "arm64-v8a",
            "ANDROID_PLATFORM": "android-21",
            "CMAKE_BUILD_TYPE": "Release",
        },
    },
)
both_builders(
    "android_aarch64_fips",
    # The Android FIPS configuration requires a newer device.
    WALLEYE_HOST,
    category = "android|aarch64",
    short_name = "fips",
    cq_compile_only = LINUX_HOST,
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "arm64-v8a",
            "ANDROID_PLATFORM": "android-21",
            # FIPS mode on Android uses shared libraries.
            "BUILD_SHARED_LIBS": "1",
            "FIPS": "1",
        },
    },
)

both_builders(
    "android_aarch64_fips_noasm",
    # The Android FIPS configuration requires a newer device.
    WALLEYE_HOST,
    category = "android|aarch64",
    short_name = "fips3",
    cq_compile_only = LINUX_HOST,
    properties = {
        "android": True,
        "cmake_args": {
            "OPENSSL_NO_ASM": "1",
            "ANDROID_ABI": "arm64-v8a",
            "ANDROID_PLATFORM": "android-21",
            # FIPS mode on Android uses shared libraries.
            "BUILD_SHARED_LIBS": "1",
            "FIPS": "1",
        },
    },
)

# delocate works on aarch64. Test this by also building the static library mode
# for android_aarch64_fips. Additionally, urandom_test doesn't work in shared
# library builds, so this gives Android FIPS coverage for urandom_test.
both_builders(
    "android_aarch64_fips_static",
    # The Android FIPS configuration requires a newer device.
    WALLEYE_HOST,
    category = "android|aarch64",
    short_name = "fips2",
    cq_compile_only = LINUX_HOST,
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "arm64-v8a",
            "ANDROID_PLATFORM": "android-21",
            "FIPS": "1",
        },
    },
)

both_builders(
    "android_arm",
    WALLEYE_HOST,
    category = "android|thumb",
    short_name = "dbg",
    cq_compile_only = LINUX_HOST,
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "armeabi-v7a",
            "ANDROID_PLATFORM": "android-21",
        },
    },
)
both_builders(
    "android_arm_rel",
    WALLEYE_HOST,
    category = "android|thumb",
    short_name = "rel",
    cq_compile_only = LINUX_HOST,
    cq_enabled = False,
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "armeabi-v7a",
            "ANDROID_PLATFORM": "android-21",
            "CMAKE_BUILD_TYPE": "Release",
            # Although Android now requires NEON support, on one builder, we
            # ignore the |__ARM_NEON| preprocessor option, to keep testing
            # non-NEON codepaths. This matters because there are a few non-NEON
            # assembly functions that would otherwise be untested.
            "CMAKE_C_FLAGS": "-DOPENSSL_NO_STATIC_NEON_FOR_TESTING=1",
            "CMAKE_CXX_FLAGS": "-DOPENSSL_NO_STATIC_NEON_FOR_TESTING=1",
        },
    },
)
both_builders(
    "android_arm_fips",
    # The Android FIPS configuration requires a newer device.
    WALLEYE_HOST,
    category = "android|thumb",
    short_name = "fips",
    cq_compile_only = LINUX_HOST,
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "armeabi-v7a",
            "ANDROID_PLATFORM": "android-21",
            # FIPS mode on Android uses shared libraries.
            "BUILD_SHARED_LIBS": "1",
            "FIPS": "1",
        },
    },
)
both_builders(
    "android_arm_armmode_rel",
    WALLEYE_HOST,
    category = "android|arm",
    short_name = "rel",
    cq_compile_only = LINUX_HOST,
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "armeabi-v7a",
            "ANDROID_ARM_MODE": "arm",
            "ANDROID_PLATFORM": "android-21",
            "CMAKE_BUILD_TYPE": "Release",
        },
    },
)
both_builders(
    "android_riscv64_compile_only",
    LINUX_HOST,
    category = "android|riscv64",
    short_name = "rel",
    properties = {
        "android": True,
        "cmake_args": {
            "ANDROID_ABI": "riscv64",
            "ANDROID_PLATFORM": "android-35",
            "CMAKE_BUILD_TYPE": "Release",
        },
        "run_unit_tests": False,
        "run_ssl_tests": False,
    },
)

both_builders("docs", LINUX_HOST, recipe = "boringssl_docs", short_name = "doc")

# For now, we use x86_64 Macs to build iOS because there are far more of them
# in luci.flex.ci and luci.flex.try pools. When this changes, switch to
# MAC_ARM64_HOST.
both_builders(
    "ios64_compile",
    MAC_X86_64_HOST,
    category = "ios",
    short_name = "64",
    properties = {
        "cmake_args": {
            "CMAKE_OSX_ARCHITECTURES": "arm64",
            "CMAKE_OSX_SYSROOT": "iphoneos",
        },
        "run_unit_tests": False,
        "run_ssl_tests": False,
    },
)
both_builders(
    "linux",
    LINUX_HOST,
    category = "linux",
    short_name = "dbg",
    properties = {
        "check_stack": True,
        "cmake_args": {
            # Pick one builder to build with the C++ runtime allowed. The default
            # configuration does not check pure virtuals
            "BORINGSSL_ALLOW_CXX_RUNTIME": "1",
            "RUST_BINDINGS": "x86_64-unknown-linux-gnu",
        },
        # Also build and test the Rust code.
        "rust": True,
    },
)
both_builders(
    "linux_rel",
    LINUX_HOST,
    category = "linux",
    short_name = "rel",
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "Release",
        },
    },
)
both_builders(
    "linux32",
    LINUX_HOST,
    category = "linux|32",
    short_name = "dbg",
    properties = {
        "check_stack": True,
        "cmake_args": {
            # 32-bit x86 is cross-compiled on the 64-bit bots.
            "CMAKE_SYSTEM_NAME": "Linux",
            "CMAKE_SYSTEM_PROCESSOR": "x86",
            "CMAKE_ASM_FLAGS": "-m32 -msse2",
            "CMAKE_CXX_FLAGS": "-m32 -msse2",
            "CMAKE_C_FLAGS": "-m32 -msse2",
        },
    },
)
both_builders(
    "linux32_rel",
    LINUX_HOST,
    category = "linux|32",
    short_name = "rel",
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "Release",
            # 32-bit x86 is cross-compiled on the 64-bit bots.
            "CMAKE_SYSTEM_NAME": "Linux",
            "CMAKE_SYSTEM_PROCESSOR": "x86",
            "CMAKE_ASM_FLAGS": "-m32 -msse2",
            "CMAKE_C_FLAGS": "-m32 -msse2",
            "CMAKE_CXX_FLAGS": "-m32 -msse2",
        },
    },
)
both_builders(
    "linux32_sde",
    LINUX_HOST,
    category = "linux|32",
    short_name = "sde",
    cq_enabled = False,
    execution_timeout = SDE_TIMEOUT,
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "RelWithAsserts",
            # 32-bit x86 is cross-compiled on the 64-bit bots.
            "CMAKE_SYSTEM_NAME": "Linux",
            "CMAKE_SYSTEM_PROCESSOR": "x86",
            "CMAKE_ASM_FLAGS": "-m32 -msse2",
            "CMAKE_C_FLAGS": "-m32 -msse2",
            "CMAKE_CXX_FLAGS": "-m32 -msse2",
        },
        "run_ssl_tests": False,
        "sde": True,
    },
)
both_builders(
    "linux32_nosse2_noasm",
    LINUX_HOST,
    category = "linux|32",
    short_name = "nosse2",
    properties = {
        "cmake_args": {
            "OPENSSL_NO_ASM": "1",
            "OPENSSL_NO_SSE2_FOR_TESTING": "1",
            # 32-bit x86 is cross-compiled on the 64-bit bots.
            "CMAKE_SYSTEM_NAME": "Linux",
            "CMAKE_SYSTEM_PROCESSOR": "x86",
            "CMAKE_ASM_FLAGS": "-m32 -msse2",
            "CMAKE_C_FLAGS": "-m32 -msse2",
            "CMAKE_CXX_FLAGS": "-m32 -msse2",
        },
    },
)
both_builders(
    "linux_clang_cfi",
    LINUX_HOST,
    category = "linux|clang",
    short_name = "cfi",
    cq_enabled = False,
    properties = {
        "clang": True,
        "cmake_args": {
            "CFI": "1",
        },
    },
)
both_builders(
    "linux_clang_rel",
    LINUX_HOST,
    category = "linux|clang",
    short_name = "rel",
    properties = {
        "clang": True,
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "Release",
        },
    },
)
both_builders(
    "linux_clang_rel_msan",
    LINUX_HOST,
    category = "linux|clang",
    short_name = "msan",
    properties = {
        "clang": True,
        "cmake_args": {
            # TODO(davidben): Should this be RelWithAsserts?
            "CMAKE_BUILD_TYPE": "Release",
            "MSAN": "1",
            "USE_CUSTOM_LIBCXX": "1",
        },
        "gclient_vars": {
            "checkout_libcxx": True,
        },
    },
)
both_builders(
    "linux_clang_rel_tsan",
    LINUX_HOST,
    category = "linux|clang",
    short_name = "tsan",
    cq_enabled = False,
    properties = {
        "clang": True,
        "cmake_args": {
            # TODO(davidben): Should this be RelWithAsserts?
            "CMAKE_BUILD_TYPE": "Release",
            "TSAN": "1",
            "USE_CUSTOM_LIBCXX": "1",
        },
        "gclient_vars": {
            "checkout_libcxx": True,
        },
        # SSL tests are all single-threaded, so running them under TSan is a
        # waste of time.
        "run_ssl_tests": False,
    },
)
both_builders(
    "linux_clang_ubsan",
    LINUX_HOST,
    category = "linux|clang",
    short_name = "ubsan",
    cq_enabled = True,
    properties = {
        "clang": True,
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "RelWithAsserts",
            "UBSAN": "1",
        },
    },
)
both_builders(
    "linux_fips",
    LINUX_HOST,
    category = "linux|fips",
    short_name = "dbg",
    properties = {
        "cmake_args": {
            "FIPS": "1",
        },
    },
)
both_builders(
    "linux_fips_rel",
    LINUX_HOST,
    category = "linux|fips",
    short_name = "rel",
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "Release",
            "FIPS": "1",
        },
    },
)
both_builders(
    "linux_fips_clang",
    LINUX_HOST,
    category = "linux|fips|clang",
    short_name = "dbg",
    properties = {
        "clang": True,
        "cmake_args": {
            "FIPS": "1",
        },
    },
)
both_builders(
    "linux_fips_clang_rel",
    LINUX_HOST,
    category = "linux|fips|clang",
    short_name = "rel",
    properties = {
        "clang": True,
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "Release",
            "FIPS": "1",
        },
    },
)
both_builders(
    "linux_fips_noasm_asan",
    LINUX_HOST,
    category = "linux|fips",
    short_name = "asan",
    properties = {
        "clang": True,
        "cmake_args": {
            "ASAN": "1",
            "FIPS": "1",
            "OPENSSL_NO_ASM": "1",
        },
    },
)
both_builders(
    "linux_fuzz",
    LINUX_HOST,
    category = "linux",
    short_name = "fuzz",
    properties = {
        "clang": True,
        "cmake_args": {
            "FUZZ": "1",
            "LIBFUZZER_FROM_DEPS": "1",
        },
        "gclient_vars": {
            "checkout_fuzzer": True,
        },
    },
)
both_builders(
    "linux_noasm_asan",
    LINUX_HOST,
    category = "linux",
    short_name = "asan",
    properties = {
        "clang": True,
        "cmake_args": {
            "ASAN": "1",
            "OPENSSL_NO_ASM": "1",
        },
    },
)

both_builders(
    "linux_nothreads",
    LINUX_HOST,
    category = "linux",
    short_name = "not",
    properties = {
        "cmake_args": {
            "CMAKE_C_FLAGS": "-DOPENSSL_NO_THREADS_CORRUPT_MEMORY_AND_LEAK_SECRETS_IF_THREADED=1",
            "CMAKE_CXX_FLAGS": "-DOPENSSL_NO_THREADS_CORRUPT_MEMORY_AND_LEAK_SECRETS_IF_THREADED=1",
        },
    },
)
both_builders(
    "linux_sde",
    LINUX_HOST,
    category = "linux",
    short_name = "sde",
    cq_enabled = False,
    execution_timeout = SDE_TIMEOUT,
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "RelWithAsserts",
        },
        "run_ssl_tests": False,
        "sde": True,
    },
)
both_builders(
    "linux_shared",
    LINUX_HOST,
    category = "linux",
    short_name = "sh",
    properties = {
        "cmake_args": {
            "BUILD_SHARED_LIBS": "1",
        },
        # The default Linux build may not depend on the C++ runtime. This is
        # easy to check when building shared libraries.
        "check_imported_libraries": True,
    },
)
both_builders(
    "linux_small",
    LINUX_HOST,
    category = "linux",
    short_name = "sm",
    properties = {
        "cmake_args": {
            "CMAKE_C_FLAGS": "-DOPENSSL_SMALL=1",
            "CMAKE_CXX_FLAGS": "-DOPENSSL_SMALL=1",
        },
    },
)
both_builders(
    "linux_nosse2_noasm",
    LINUX_HOST,
    category = "linux",
    short_name = "nosse2",
    properties = {
        "cmake_args": {
            "OPENSSL_NO_ASM": "1",
            "OPENSSL_NO_SSE2_FOR_TESTING": "1",
        },
    },
)
both_builders(
    "linux_bazel",
    LINUX_HOST,
    category = "linux",
    short_name = "bzl",
    recipe = "boringssl_bazel",
)
both_builders(
    "mac",
    MAC_X86_64_HOST,
    category = "mac",
    short_name = "dbg",
    properties = {
        "cmake_args": {
            "RUST_BINDINGS": "x86_64-apple-darwin",
        },
        # Also build and test the Rust code.
        "rust": True,
    },
)
both_builders(
    "mac_rel",
    MAC_X86_64_HOST,
    category = "mac",
    short_name = "rel",
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "Release",
        },
    },
)
both_builders(
    "mac_small",
    MAC_X86_64_HOST,
    category = "mac",
    short_name = "sm",
    properties = {
        "cmake_args": {
            "CMAKE_C_FLAGS": "-DOPENSSL_SMALL=1",
            "CMAKE_CXX_FLAGS": "-DOPENSSL_SMALL=1",
        },
    },
)
both_builders(
    "mac_arm64",
    MAC_ARM64_HOST,
    category = "mac",
    short_name = "arm64",
    properties = {
        "cmake_args": {
            "RUST_BINDINGS": "aarch64-apple-darwin",
        },
        # Also build and test the Rust code.
        "rust": True,
    },
)
both_builders(
    "mac_arm64_bazel",
    MAC_ARM64_HOST,
    category = "mac",
    short_name = "bzl",
    recipe = "boringssl_bazel",
)
both_builders(
    "win32",
    WIN_HOST,
    category = "win|x86",
    short_name = "dbg",
    cq_compile_only = WIN_HOST,  # Reduce CQ cycle times.
    properties = {
        "msvc_target": "x86",
    },
)
both_builders(
    "win32_rel",
    WIN_HOST,
    category = "win|x86",
    short_name = "rel",
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "Release",
        },
        "msvc_target": "x86",
    },
)
both_builders(
    "win32_sde",
    WIN_HOST,
    category = "win|x86",
    short_name = "sde",
    cq_enabled = False,
    execution_timeout = SDE_TIMEOUT,
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "RelWithAsserts",
        },
        "msvc_target": "x86",
        "run_ssl_tests": False,
        "sde": True,
    },
)
both_builders(
    "win32_small",
    WIN_HOST,
    category = "win|x86",
    short_name = "sm",
    properties = {
        "cmake_args": {
            "CMAKE_C_FLAGS": "-DOPENSSL_SMALL=1",
            "CMAKE_CXX_FLAGS": "-DOPENSSL_SMALL=1",
        },
        "msvc_target": "x86",
    },
)

both_builders(
    "win32_clang",
    WIN_HOST,
    category = "win|x86",
    short_name = "clang",
    cq_compile_only = WIN_HOST,  # Reduce CQ cycle times.
    properties = {
        "clang": True,
        "msvc_target": "x86",
        "cmake_args": {
            # Clang doesn't pick up 32-bit x86 from msvc_target. Specify it as a
            # cross-compile.
            "CMAKE_SYSTEM_NAME": "Windows",
            "CMAKE_SYSTEM_PROCESSOR": "x86",
            "CMAKE_ASM_FLAGS": "-m32 -msse2",
            "CMAKE_C_FLAGS": "-m32 -msse2",
            "CMAKE_CXX_FLAGS": "-m32 -msse2",
        },
    },
)

both_builders(
    "win64",
    WIN_HOST,
    category = "win|x64",
    short_name = "dbg",
    cq_compile_only = WIN_HOST,  # Reduce CQ cycle times.
    properties = {
        "msvc_target": "x64",
    },
)

both_builders(
    "win64_rel",
    WIN_HOST,
    category = "win|x64",
    short_name = "rel",
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "Release",
            "RUST_BINDINGS": "x86_64-pc-windows-msvc",
        },
        "msvc_target": "x64",
        # Also build and test the Rust code.
        "rust": True,
    },
)
both_builders(
    "win64_sde",
    WIN_HOST,
    category = "win|x64",
    short_name = "sde",
    cq_enabled = False,
    execution_timeout = SDE_TIMEOUT,
    properties = {
        "cmake_args": {
            "CMAKE_BUILD_TYPE": "RelWithAsserts",
        },
        "msvc_target": "x64",
        "run_ssl_tests": False,
        "sde": True,
    },
)
both_builders(
    "win64_small",
    WIN_HOST,
    category = "win|x64",
    short_name = "sm",
    properties = {
        "cmake_args": {
            "CMAKE_C_FLAGS": "-DOPENSSL_SMALL=1",
            "CMAKE_CXX_FLAGS": "-DOPENSSL_SMALL=1",
        },
        "msvc_target": "x64",
    },
)

both_builders(
    "win64_clang",
    WIN_HOST,
    category = "win|x64",
    short_name = "clg",
    cq_compile_only = WIN_HOST,  # Reduce CQ cycle times.
    properties = {
        "clang": True,
        "msvc_target": "x64",
    },
)

both_builders(
    "win_arm64_compile",
    WIN_HOST,
    category = "win|arm64",
    short_name = "clang",
    properties = {
        "clang": True,
        "cmake_args": {
            # Clang doesn't pick up arm64 from msvc_target. Specify it as a
            # cross-compile.
            "CMAKE_SYSTEM_NAME": "Windows",
            "CMAKE_SYSTEM_PROCESSOR": "arm64",
            "CMAKE_ASM_FLAGS": "--target=arm64-windows",
            "CMAKE_C_FLAGS": "--target=arm64-windows",
            "CMAKE_CXX_FLAGS": "--target=arm64-windows",
        },
        "gclient_vars": {
            "checkout_nasm": False,
        },
        "msvc_target": "arm64",
        "run_unit_tests": False,
        "run_ssl_tests": False,
    },
)

both_builders(
    "win_arm64_msvc_compile",
    WIN_HOST,
    category = "win|arm64",
    short_name = "msvc",
    properties = {
        "cmake_args": {
            # This is a cross-compile, so CMake needs to be told the processor.
            # MSVC will pick up the architecture from msvc_target.
            "CMAKE_SYSTEM_NAME": "Windows",
            "CMAKE_SYSTEM_PROCESSOR": "arm64",
            # We do not currently support Windows arm64 assembly with MSVC.
            "OPENSSL_NO_ASM": "1",
        },
        "gclient_vars": {
            "checkout_nasm": False,
        },
        "msvc_target": "arm64",
        "run_unit_tests": False,
        "run_ssl_tests": False,
    },
)
