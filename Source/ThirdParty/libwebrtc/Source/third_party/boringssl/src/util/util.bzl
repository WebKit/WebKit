# Copyright (c) 2024, Google Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# Configure C, C++, and common flags for GCC-compatible toolchains.
#
# TODO(davidben): Can we remove some of these? In Bazel, are warnings the
# toolchain or project's responsibility? -fno-common did not become default
# until https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85678.
gcc_copts = [
    # This list of warnings should match those in the top-level CMakeLists.txt.
    "-Wall",
    "-Werror",
    "-Wformat=2",
    "-Wsign-compare",
    "-Wmissing-field-initializers",
    "-Wwrite-strings",
    "-Wshadow",
    "-fno-common",
]

gcc_copts_cxx = [
    "-Wmissing-declarations",
]

gcc_copts_c = [
    "-Wmissing-prototypes",
    "-Wold-style-definition",
    "-Wstrict-prototypes",
]

boringssl_copts_common = select({
    # This condition and the asm_srcs_used one below must be kept in sync.
    "@platforms//os:windows": ["-DOPENSSL_NO_ASM"],
    "//conditions:default": [],
}) + select({
    # We assume that non-Windows builds use a GCC-compatible toolchain and that
    # Windows builds do not.
    #
    # TODO(davidben): Should these be querying something in @bazel_tools?
    # Unfortunately, @bazel_tools is undocumented. See
    # https://github.com/bazelbuild/bazel/issues/14914
    "@platforms//os:windows": [],
    "//conditions:default": gcc_copts,
}) + select({
    # This is needed on glibc systems to get rwlock in pthreads, but it should
    # not be set on Apple platforms or FreeBSD, where it instead disables APIs
    # we use.
    # See compat(5), sys/cdefs.h, and https://crbug.com/boringssl/471
    "@platforms//os:linux": ["-D_XOPEN_SOURCE=700"],
    # Without WIN32_LEAN_AND_MEAN, <windows.h> pulls in wincrypt.h, which
    # conflicts with our <openssl/x509.h>.
    "@platforms//os:windows": ["-DWIN32_LEAN_AND_MEAN", "-utf-8"],
    "//conditions:default": [],
})

# We do not specify the C++ version here because Bazel expects C++ version
# to come from the top-level toolchain. The concern is that different C++
# versions may cause ABIs, notably Abseil's, to change.
boringssl_copts_cxx = boringssl_copts_common + select({
    "@platforms//os:windows": [],
    "//conditions:default": gcc_copts_cxx,
})

# We specify the C version because Bazel projects often do not remember to
# specify the C version. We do not expect ABIs to vary by C versions, at least
# for our code or the headers we include, so configure the C version inside the
# library. If Bazel's C/C++ version handling improves, we may reconsider this.
boringssl_copts_c = boringssl_copts_common + select({
    "@platforms//os:windows": ["/std:c11"],
    "//conditions:default": ["-std=c11"] + gcc_copts_c,
})

def handle_mixed_c_cxx(
        name,
        copts,
        deps,
        internal_hdrs,
        includes,
        linkopts,
        srcs,
        testonly):
    """
    Works around https://github.com/bazelbuild/bazel/issues/22041. Determines
    whether a target contains C, C++, or both. If the target is multi-language,
    the C sources are split into a separate library. Returns a tuple of updated
    (copts, deps, srcs) to apply.
    """
    has_c, has_cxx = False, False
    for src in srcs:
        if src.endswith(".c"):
            has_c = True
        elif src.endswith(".cc"):
            has_cxx = True

    # If a target has both C and C++, we need to split it in two.
    if has_c and has_cxx:
        srcs_c = [src for src in srcs if src.endswith(".c") or src.endswith(".h")]
        name_c = name + "_c"
        cc_library(
            name = name_c,
            srcs = srcs_c + internal_hdrs,
            copts = copts + boringssl_copts_c,
            includes = includes,
            # This target only exists to be linked into the main library, so
            # always link it statically.
            linkstatic = True,
            linkopts = linkopts,
            deps = deps,
            testonly = testonly,
        )

        # Build the remainder as a C++-only target.
        deps += [":" + name_c]
        srcs = [src for src in srcs if not src.endswith(".c")]
        has_c = False

    if has_c:
        copts += boringssl_copts_c
    else:
        copts += boringssl_copts_cxx

    return copts, deps, srcs

def handle_asm_srcs(asm_srcs):
    if not asm_srcs:
        return []

    # By default, the C files will expect assembly files, if any, to be linked
    # in with the build. This default can be flipped with -DOPENSSL_NO_ASM. If
    # building in a configuration where we have no assembly optimizations,
    # -DOPENSSL_NO_ASM has no effect, and either value is fine.
    #
    # Like C files, assembly files are wrapped in #ifdef (or NASM equivalent),
    # so it is safe to include a file for the wrong platform in the build. It
    # will just output an empty object file. However, we need some platform
    # selectors to distinguish between gas or NASM syntax.
    #
    # For all non-Windows platforms, we use gas assembly syntax and can assume
    # any GCC-compatible toolchain includes a gas-compatible assembler.
    #
    # For Windows, we use NASM on x86 and x86_64 and gas, specifically
    # clang-assembler, on aarch64. We have not yet added NASM support to this
    # build, and would need to detect MSVC vs clang-cl for aarch64 so, for now,
    # we just disable assembly on Windows across the board.
    #
    # This select and the corresponding one in boringssl_copts_common must be
    # kept in sync.
    #
    # TODO(https://crbug.com/boringssl/531): Enable assembly for Windows.
    return select({
        "@platforms//os:windows": [],
        "//conditions:default": asm_srcs,
    })

def linkstatic_kwargs(linkstatic):
    # Although Bazel's documentation says linkstatic defaults to True or False
    # for the various target types, this is not true. The defaults differ by
    # platform non-Windows and True on Windows. There is now way to request the
    # default except to omit the parameter, so we must use kwargs.
    kwargs = {}
    if linkstatic != None:
        kwargs["linkstatic"] = linkstatic
    return kwargs

def bssl_cc_library(
        name,
        asm_srcs = [],
        copts = [],
        deps = [],
        hdrs = [],
        includes = [],
        internal_hdrs = [],
        linkopts = [],
        linkstatic = None,
        srcs = [],
        testonly = False,
        visibility = []):
    copts, deps, srcs = handle_mixed_c_cxx(
        name = name,
        copts = copts,
        deps = deps,
        internal_hdrs = hdrs + internal_hdrs,
        includes = includes,
        linkopts = linkopts,
        srcs = srcs,
        testonly = testonly,
    )

    # BoringSSL's notion of internal headers are slightly different from
    # Bazel's. libcrypto's internal headers may be used by libssl, but they
    # cannot be used outside the library. To express this, we make separate
    # internal and external targets. This impact's Bazel's layering check.
    name_internal = name
    if visibility:
        name_internal = name + "_internal"

    cc_library(
        name = name_internal,
        srcs = srcs + handle_asm_srcs(asm_srcs),
        hdrs = hdrs + internal_hdrs,
        copts = copts,
        includes = includes,
        linkopts = linkopts,
        deps = deps,
        testonly = testonly,
        **linkstatic_kwargs(linkstatic)
    )

    if visibility:
        cc_library(
            name = name,
            hdrs = hdrs,
            deps = [":" + name_internal],
            visibility = visibility,
        )

def bssl_cc_binary(
        name,
        srcs = [],
        asm_srcs = [],
        copts = [],
        includes = [],
        linkstatic = None,
        linkopts = [],
        deps = [],
        testonly = False,
        visibility = []):
    copts, deps, srcs = handle_mixed_c_cxx(
        name = name,
        copts = copts,
        deps = deps,
        internal_hdrs = [],
        includes = includes,
        linkopts = linkopts,
        srcs = srcs,
        testonly = testonly,
    )

    cc_binary(
        name = name,
        srcs = srcs + handle_asm_srcs(asm_srcs),
        copts = copts,
        includes = includes,
        linkopts = linkopts,
        deps = deps,
        testonly = testonly,
        visibility = visibility,
        **linkstatic_kwargs(linkstatic)
    )

def bssl_cc_test(
        name,
        srcs = [],
        asm_srcs = [],
        data = [],
        size = "medium",
        internal_hdrs = [],
        copts = [],
        includes = [],
        linkopts = [],
        linkstatic = None,
        deps = [],
        shard_count = None):
    copts, deps, srcs = handle_mixed_c_cxx(
        name = name,
        copts = copts,
        deps = deps,
        internal_hdrs = [],
        includes = includes,
        linkopts = linkopts,
        srcs = srcs,
        testonly = True,
    )

    cc_test(
        name = name,
        data = data,
        deps = deps,
        srcs = srcs + handle_asm_srcs(asm_srcs),
        copts = copts,
        includes = includes,
        linkopts = linkopts,
        shard_count = shard_count,
        size = size,
        **linkstatic_kwargs(linkstatic)
    )
