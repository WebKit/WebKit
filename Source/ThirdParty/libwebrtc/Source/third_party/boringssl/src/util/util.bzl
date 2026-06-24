# Copyright 2024 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# Configure C, C++, and common flags for GCC-compatible toolchains.
#
# TODO(davidben): Can we remove some of these? In Bazel, are warnings the
# toolchain or project's responsibility? -fno-common did not become default
# until https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85678.
gcc_copts = [
    # This list of warnings should match those in the top-level CMakeLists.txt.
    "-Wall",
    "-Wformat=2",
    "-Wmissing-field-initializers",
    "-Wshadow",
    "-Wsign-compare",
    "-Wtype-limits",
    "-Wvla",
    "-Wwrite-strings",
    "-fno-common",
    "-fno-strict-aliasing",
]

gcc_cxxopts = [
    "-Wmissing-declarations",
    "-Wnon-virtual-dtor",
]

gcc_conlyopts = [
    "-Wmissing-prototypes",
    "-Wold-style-definition",
    "-Wstrict-prototypes",
]

boringssl_copts = select({
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
boringssl_cxxopts = select({
    "@platforms//os:windows": [],
    "//conditions:default": gcc_cxxopts,
})

# We specify the C version because Bazel projects often do not remember to
# specify the C version. We do not expect ABIs to vary by C versions, at least
# for our code or the headers we include, so configure the C version inside the
# library. If Bazel's C/C++ version handling improves, we may reconsider this.
boringssl_conlyopts = select({
    "@platforms//os:windows": ["/std:c11"],
    "//conditions:default": ["-std=c11"] + gcc_conlyopts,
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

def bssl_cc_library(
        name,
        asm_srcs = [],
        copts = [],
        deps = [],
        implementation_deps = [],
        hdrs = [],
        includes = [],
        internal_hdrs = [],
        linkopts = [],
        linkstatic = None,
        srcs = [],
        testonly = False,
        alwayslink = False,
        visibility = []):
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
        copts = copts + boringssl_copts,
        conlyopts = boringssl_conlyopts,
        cxxopts = boringssl_cxxopts,
        includes = includes,
        linkopts = linkopts,
        deps = deps,
        implementation_deps = implementation_deps,
        testonly = testonly,
        alwayslink = alwayslink,
        **linkstatic_kwargs(linkstatic)
    )

    if visibility:
        cc_library(
            name = name,
            hdrs = hdrs,
            # Depend on the internal target via implementation_deps to avoid
            # re-exporting internal_hdrs.
            implementation_deps = [":" + name_internal],
            # Although picked up transitively, re-specify deps and includes, so
            # that targets depending on the public target also pick them up.
            deps = deps,
            includes = includes,
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
    cc_binary(
        name = name,
        srcs = srcs + handle_asm_srcs(asm_srcs),
        copts = copts + boringssl_copts,
        conlyopts = boringssl_conlyopts,
        cxxopts = boringssl_cxxopts,
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
        copts = [],
        includes = [],
        linkopts = [],
        linkstatic = None,
        deps = [],
        shard_count = None):
    cc_test(
        name = name,
        data = data,
        deps = deps,
        srcs = srcs + handle_asm_srcs(asm_srcs),
        copts = copts + boringssl_copts,
        conlyopts = boringssl_conlyopts,
        cxxopts = boringssl_cxxopts,
        includes = includes,
        linkopts = linkopts,
        shard_count = shard_count,
        size = size,
        **linkstatic_kwargs(linkstatic)
    )
