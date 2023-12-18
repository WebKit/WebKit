# Copyright (c) 2016, Google Inc.
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
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load(
    ":BUILD.generated.bzl",
    "crypto_headers",
    "crypto_internal_headers",
    "crypto_sources",
    "crypto_sources_asm",
    "fips_fragments",
    "ssl_headers",
    "ssl_internal_headers",
    "ssl_sources",
    "tool_headers",
    "tool_sources",
)

licenses(["notice"])

exports_files(["LICENSE"])

# By default, the C files will expect assembly files, if any, to be linked in
# with the build. This default can be flipped with -DOPENSSL_NO_ASM. If building
# in a configuration where we have no assembly optimizations, -DOPENSSL_NO_ASM
# has no effect, and either value is fine.
#
# Like C files, assembly files are wrapped in #ifdef (or NASM equivalent), so it
# is safe to include a file for the wrong platform in the build. It will just
# output an empty object file. However, we need some platform selectors to
# distinguish between gas or NASM syntax.
#
# For all non-Windows platforms, we use gas assembly syntax and can assume any
# GCC-compatible toolchain includes a gas-compatible assembler.
#
# For Windows, we use NASM on x86 and x86_64 and gas, specifically
# clang-assembler, on aarch64. We have not yet added NASM support to this build,
# and would need to detect MSVC vs clang-cl for aarch64 so, for now, we just
# disable assembly on Windows across the board.
#
# These two selects for asm_sources and asm_copts must be kept in sync. If we
# specify assembly, we don't want OPENSSL_NO_ASM. If we don't specify assembly,
# we want OPENSSL_NO_ASM, in case the C files expect them in some format (e.g.
# NASM) this build file doesn't yet support.
#
# TODO(https://crbug.com/boringssl/531): Enable assembly for Windows.
asm_sources = select({
    "@platforms//os:windows": [],
    "//conditions:default": crypto_sources_asm,
})
asm_copts = select({
    "@platforms//os:windows": ["-DOPENSSL_NO_ASM"],
    "//conditions:default": [],
})

# Configure C, C++, and common flags for GCC-compatible toolchains.
#
# TODO(davidben): Can we remove some of these? In Bazel, are warnings the
# toolchain or project's responsibility? -Wa,--noexecstack should be unnecessary
# now, though https://crbug.com/boringssl/292 tracks testing this in CI.
# -fno-common did not become default until
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85678.
gcc_copts = [
    # Assembler option --noexecstack adds .note.GNU-stack to each object to
    # ensure that binaries can be built with non-executable stack.
    "-Wa,--noexecstack",

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
gcc_copts_c11 = [
    "-std=c11",
    "-Wmissing-prototypes",
    "-Wold-style-definition",
    "-Wstrict-prototypes",
]
gcc_copts_cxx = [
    "-std=c++14",
    "-Wmissing-declarations",
]

boringssl_copts = [
    "-DBORINGSSL_IMPLEMENTATION",
] + select({
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
    "@platforms//os:windows": ["-DWIN32_LEAN_AND_MEAN"],
    "//conditions:default": [],
}) + asm_copts

boringssl_copts_c11 = boringssl_copts + select({
    "@platforms//os:windows": ["/std:c11"],
    "//conditions:default": gcc_copts_c11,
})

boringssl_copts_cxx = boringssl_copts + select({
    "@platforms//os:windows": [],
    "//conditions:default": gcc_copts_cxx,
})

cc_library(
    name = "crypto",
    srcs = crypto_sources + crypto_internal_headers + asm_sources,
    hdrs = crypto_headers + fips_fragments,
    copts = boringssl_copts_c11,
    includes = ["src/include"],
    linkopts = select({
        "@platforms//os:windows": ["-defaultlib:advapi32.lib"],
        "//conditions:default": ["-pthread"],
    }),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ssl",
    srcs = ssl_sources + ssl_internal_headers,
    hdrs = ssl_headers,
    copts = boringssl_copts_cxx,
    includes = ["src/include"],
    visibility = ["//visibility:public"],
    deps = [
        ":crypto",
    ],
)

cc_binary(
    name = "bssl",
    srcs = tool_sources + tool_headers,
    copts = boringssl_copts_cxx,
    visibility = ["//visibility:public"],
    deps = [":ssl"],
)
