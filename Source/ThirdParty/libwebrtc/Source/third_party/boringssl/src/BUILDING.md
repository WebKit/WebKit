# Building BoringSSL

## Checking out BoringSSL

    git clone "https://boringssl.googlesource.com/boringssl"

## Build Prerequisites

The standalone CMake build is primarily intended for developers. If embedding
BoringSSL into another project with a pre-existing build system, see
[INCORPORATING.md](./INCORPORATING.md).

Unless otherwise noted, build tools must at most five years old, matching
[Abseil guidelines](https://abseil.io/about/compatibility). If in doubt, use the
most recent stable version of each tool.

  * [CMake](https://cmake.org/download/) 3.22 or later is required.

  * Building with [Ninja](https://ninja-build.org/) instead of Make is
    recommended, because it makes builds faster. On Windows, CMake's Visual
    Studio generator may also work, but it not tested regularly and requires
    recent versions of CMake for assembly support.

  * On Windows only, [NASM](https://www.nasm.us/) is required. If not found
    by CMake, it may be configured explicitly by setting
    `CMAKE_ASM_NASM_COMPILER`.

  * Compilers for C11 and C++17, or later, are required. On Windows, MSVC from
    Visual Studio 2022 or later with Windows 10 SDK 2104 or later are
    supported, but using the latest versions is recommended. Recent versions of
    GCC (6.1+) and Clang should work on non-Windows platforms, and maybe on
    Windows too.

## Building

Using Ninja (note the 'N' is capitalized in the cmake invocation):

    cmake -GNinja -B build
    ninja -C build

Using Make (does not work on Windows):

    cmake -B build
    make -C build

This produces a debug build by default. Optimisation isn't enabled, and debug
assertions are included. Pass `-DCMAKE_BUILD_TYPE=Release` to `cmake` to
configure a release build:

    cmake -GNinja -B build -DCMAKE_BUILD_TYPE=Release
    ninja -C build

If you want to cross-compile then there is an example toolchain file for 32-bit
Intel in `util/`. Wipe out the build directory, run `cmake` like this:

    cmake -B build -DCMAKE_TOOLCHAIN_FILE=../util/32-bit-toolchain.cmake -GNinja

If you want to build as a shared library, pass `-DBUILD_SHARED_LIBS=1`. On
Windows, where functions need to be tagged with `dllimport` when coming from a
shared library, define `BORINGSSL_SHARED_LIBRARY` in any code which `#include`s
the BoringSSL headers.

In order to serve environments where code-size is important as well as those
where performance is the overriding concern, `OPENSSL_SMALL` can be defined to
remove some code that is especially large.

See [CMake's documentation](https://cmake.org/cmake/help/v3.4/manual/cmake-variables.7.html)
for other variables which may be used to configure the build.

You usually don't need to run `cmake` again after changing `CMakeLists.txt`
files because the build scripts will detect changes to them and rebuild
themselves automatically.

### Building for Android

It's possible to build BoringSSL with the Android NDK using CMake. Recent
versions of the NDK include a CMake toolchain file. This has been tested with
version r16b of the NDK.

Unpack the Android NDK somewhere and export `ANDROID_NDK` to point to the
directory. Then run CMake like this:

    cmake -DANDROID_ABI=arm64-v8a \
          -DANDROID_PLATFORM=android-21 \
          -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
          -GNinja -B build

Once you've run that, Ninja should produce Android-compatible binaries.  You
can replace `arm64-v8a` with `armeabi-v7a` in the above to build aarch64
binaries.

For other options, see the documentation in the toolchain file.

To run tests, you can use the `util/run_android_tests.go` script. Individual
binaries can also be pushed to the device with `adb`, but note that some tests
require test data. The `-upload-only` flag to `run_android_tests.go` may be
useful.

To debug the resulting binaries on an Android device with `gdb`, run the
commands below. Replace `ARCH` with the architecture of the target device, e.g.
`arm` or `arm64`.

    adb push ${ANDROID_NDK}/prebuilt/android-ARCH/gdbserver/gdbserver \
        /data/local/tmp
    adb forward tcp:5039 tcp:5039
    adb shell /data/local/tmp/gdbserver :5039 /path/on/device/to/binary

Then run the following in a separate shell. Replace `HOST` with the OS and
architecture of the host machine, e.g. `linux-x86_64`.

    ${ANDROID_NDK}/prebuilt/HOST/bin/gdb
    target remote :5039  # in gdb

### Building for iOS

To build for iOS, pass `-DCMAKE_OSX_SYSROOT=iphoneos` and
`-DCMAKE_OSX_ARCHITECTURES=ARCH` to CMake, where `ARCH` is the desired
architecture, matching values used in the `-arch` flag in Apple's toolchain.

Passing multiple architectures for a multiple-architecture build is not
supported.

### Building with Prefixed Symbols

BoringSSL's build system has experimental support for adding a custom prefix to
all symbols. This can be useful when linking multiple versions of BoringSSL in
the same project to avoid symbol conflicts.

In order to build with prefixed symbols, the `BORINGSSL_PREFIX` CMake variable
should specify the prefix to add to all symbols. In other words, `cmake -B build
-DBORINGSSL_PREFIX=MY_CUSTOM_PREFIX` will configure the build to add
the prefix `MY_CUSTOM_PREFIX` to all of the symbols defined by the library.

Note that symbol prefixing cannot be used with the combination of FIPS and
static libraries.

This mechanism is under development and may change over time. Please contact the
BoringSSL maintainers if making use of it.

## Known Limitations on Windows

  * CMake can generate Visual Studio projects, but the generated project files
    don't have steps for assembling the assembly language source files, so they
    currently cannot be used to build BoringSSL.

## ARM CPU Capabilities

ARM, unlike Intel, does not have a userspace instruction that allows
applications to discover the capabilities of the processor. Instead, the
capability information has to be provided by a combination of compile-time
information and the operating system.

BoringSSL determines capabilities at compile-time based on `__ARM_NEON`,
`__ARM_FEATURE_AES`, and other preprocessor symbols defined in
[Arm C Language Extensions (ACLE)](https://developer.arm.com/architectures/system-architectures/software-standards/acle).
These values are usually controlled by the `-march` flag. You can also define
any of the following to enable the corresponding ARM feature, but using the ACLE
symbols via `-march` is recommended.

  * `OPENSSL_STATIC_ARMCAP_NEON`
  * `OPENSSL_STATIC_ARMCAP_AES`
  * `OPENSSL_STATIC_ARMCAP_SHA1`
  * `OPENSSL_STATIC_ARMCAP_SHA256`
  * `OPENSSL_STATIC_ARMCAP_PMULL`

The resulting binary will assume all such features are always present. This can
reduce code size, by allowing the compiler to omit fallbacks. However, if the
feature is not actually supported at runtime, BoringSSL will likely crash.

BoringSSL will additionally query the operating system at runtime for additional
features, e.g. with `getauxval` on Linux. This allows a single binary to use
newer instructions when present, but still function on CPUs without them. But
some environments don't support runtime queries. If building for those, define
`OPENSSL_STATIC_ARMCAP` to limit BoringSSL to compile-time capabilities. If not
defined, the target operating system must be known to BoringSSL.

## Binary Size

The implementations of some algorithms require a trade-off between binary size
and performance. For instance, BoringSSL's fastest P-256 implementation uses a
148 KiB pre-computed table. To optimize instead for binary size, pass
`-DOPENSSL_SMALL=1` to CMake or define the `OPENSSL_SMALL` preprocessor symbol.

# Running Tests

There are two additional dependencies for running tests:

  * The most recent stable version of [Go](https://go.dev/) is required.
    Note Go is exempt from the five year support window. If not found by CMake,
    the go executable may be configured explicitly by setting `GO_EXECUTABLE`.

  * On x86_64 Linux, the tests have an optional
    [libunwind](https://www.nongnu.org/libunwind/) dependency to test the
    assembly more thoroughly.

There are two sets of tests: the C/C++ tests and the blackbox tests. For former
are built by Ninja and can be run from the top-level directory with `go run
util/all_tests.go`. The latter have to be run separately by running `go test`
from within `ssl/test/runner`.

Both sets of tests may also be run with `ninja -C build run_tests`, but CMake
3.2 or later is required to avoid Ninja's output buffering.

# Running Benchmarks

To invoke the benchmarks, execute the `bssl_bench` binary.
You have the option to enumerate all of the benchmarks first by passing in
`--benchmark_list_tests`.
To run specific benchmarks, you may pass in `--benchmark_filter=$regex` where
`$regex` is an [ECMAScript regular expression].

You can specify a set of input sizes in bytes for relevant cipher suites
with the repeatable flag `-i` or `--input-size`.
You can let the benchmark run with a set of various thread counts with the
repeatable flag `-t` or `--threads`.
For more benchmark configuration options, you may read the manual by passing in
`--help` flag on launch.
For more information on `--benchmark_*` flags, we refer you to the
[benchmark user guide].

There are also [additional tools] at your disposal to interpret the benchmark
results.

[ECMAScript regular expression]: https://cppreference.com/w/cpp/regex/ecmascript.html
[benchmark user guide]: https://google.github.io/benchmark/user_guide.html
[additional tools]: https://google.github.io/benchmark/tools.html

# Pre-generated Files

If modifying perlasm files, `util/pregenerate/build.json`, or adding new public
symbols, you will need to run `go run ./util/pregenerate` to refresh some
pre-generated files. To do this, you will need:

- The most recent stable version of [Go](https://go.dev/)

- A recent version of [Perl](https://www.perl.org/)

- A recent version of [Clang](https://clang.llvm.org/)

If not available in `PATH`, the `-perl=path/to/perl` and `-clang=path/to/clang`
flags can be used to specify where Perl and Clang can be found, respectively.
Passing an empty string (`-perl=` and `-clang=`) will skip these dependencies,
but not all files will be regenerated. If any skipped files need to be updated,
some of your changes may not be applied to your build, and your change may fail
tests on our CI.

On non-Windows systems, Perl and Clang are either installed by default or
readily available from package managers or Xcode.

On Windows, Clang can be installed from Visual Studio. Perl can be installed
from [Strawberry Perl](https://strawberryperl.com/). Prefer to use the
"Portable zip" version. The MSI installer will
[add GCC to `PATH`](https://github.com/StrawberryPerl/Perl-Dist-Strawberry/issues/11),
which can confuse some build tools when identifying the compiler. If this has
happened, removing `C:\Strawberry\c\bin` from `PATH` should resolve any
problems. [`CMAKE_IGNORE_PATH`](https://cmake.org/cmake/help/latest/variable/CMAKE_IGNORE_PATH.html)
may also be useful.

See [gen/README.md](./gen/README.md) for more details.
