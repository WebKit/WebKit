# Building BoringSSL

## Build Prerequisites

  * [CMake](https://cmake.org/download/) 2.8.11 or later is required.

  * Perl 5.6.1 or later is required. On Windows,
    [Active State Perl](http://www.activestate.com/activeperl/) has been
    reported to work, as has MSYS Perl.
    [Strawberry Perl](http://strawberryperl.com/) also works but it adds GCC
    to `PATH`, which can confuse some build tools when identifying the compiler
    (removing `C:\Strawberry\c\bin` from `PATH` should resolve any problems).
    If Perl is not found by CMake, it may be configured explicitly by setting
    `PERL_EXECUTABLE`.

  * On Windows you currently must use [Ninja](https://ninja-build.org/)
    to build; on other platforms, it is not required, but recommended, because
    it makes builds faster.

  * If you need to build Ninja from source, then a recent version of
    [Python](https://www.python.org/downloads/) is required (Python 2.7.5 works).

  * On Windows only, [NASM](https://www.nasm.us/) is required. If not found
    by CMake, it may be configured explicitly by setting
    `CMAKE_ASM_NASM_COMPILER`.

  * A C compiler is required. On Windows, MSVC 14 (Visual Studio 2015) or later
    with Platform SDK 8.1 or later are supported. Recent versions of GCC (4.8+)
    and Clang should work on non-Windows platforms, and maybe on Windows too.
    To build the tests, you also need a C++ compiler with C++11 support.

  * The most recent stable version of [Go](https://golang.org/dl/) is required.
    If not found by CMake, the go executable may be configured explicitly by
    setting `GO_EXECUTABLE`.

  * To build the x86 and x86\_64 assembly, your assembler must support AVX2
    instructions and MOVBE. If using GNU binutils, you must have 2.22 or later

## Building

Using Ninja (note the 'N' is capitalized in the cmake invocation):

    mkdir build
    cd build
    cmake -GNinja ..
    ninja

Using Make (does not work on Windows):

    mkdir build
    cd build
    cmake ..
    make

You usually don't need to run `cmake` again after changing `CMakeLists.txt`
files because the build scripts will detect changes to them and rebuild
themselves automatically.

Note that the default build flags in the top-level `CMakeLists.txt` are for
debugging—optimisation isn't enabled. Pass `-DCMAKE_BUILD_TYPE=Release` to
`cmake` to configure a release build.

If you want to cross-compile then there is an example toolchain file for 32-bit
Intel in `util/`. Wipe out the build directory, recreate it and run `cmake` like
this:

    cmake -DCMAKE_TOOLCHAIN_FILE=../util/32-bit-toolchain.cmake -GNinja ..

If you want to build as a shared library, pass `-DBUILD_SHARED_LIBS=1`. On
Windows, where functions need to be tagged with `dllimport` when coming from a
shared library, define `BORINGSSL_SHARED_LIBRARY` in any code which `#include`s
the BoringSSL headers.

In order to serve environments where code-size is important as well as those
where performance is the overriding concern, `OPENSSL_SMALL` can be defined to
remove some code that is especially large.

See [CMake's documentation](https://cmake.org/cmake/help/v3.4/manual/cmake-variables.7.html)
for other variables which may be used to configure the build.

### Building for Android

It's possible to build BoringSSL with the Android NDK using CMake. Recent
versions of the NDK include a CMake toolchain file which works with CMake 3.6.0
or later. This has been tested with version r16b of the NDK.

Unpack the Android NDK somewhere and export `ANDROID_NDK` to point to the
directory. Then make a build directory as above and run CMake like this:

    cmake -DANDROID_ABI=armeabi-v7a \
          -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
          -DANDROID_NATIVE_API_LEVEL=16 \
          -GNinja ..

Once you've run that, Ninja should produce Android-compatible binaries.  You
can replace `armeabi-v7a` in the above with `arm64-v8a` and use API level 21 or
higher to build aarch64 binaries.

For older NDK versions, BoringSSL ships a third-party CMake toolchain file. Use
`../third_party/android-cmake/android.toolchain.cmake` for
`CMAKE_TOOLCHAIN_FILE` instead.

For other options, see the documentation in the toolchain file.

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
should specify the prefix to add to all symbols, and the
`BORINGSSL_PREFIX_SYMBOLS` CMake variable should specify the path to a file
which contains a list of symbols which should be prefixed (one per line;
comments are supported with `#`). In other words, `cmake ..
-DBORINGSSL_PREFIX=MY_CUSTOM_PREFIX
-DBORINGSSL_PREFIX_SYMBOLS=/path/to/symbols.txt` will configure the build to add
the prefix `MY_CUSTOM_PREFIX` to all of the symbols listed in
`/path/to/symbols.txt`.

It is currently the caller's responsibility to create and maintain the list of
symbols to be prefixed. Alternatively, `util/read_symbols.go` reads the list of
exported symbols from a `.a` file, and can be used in a build script to generate
the symbol list on the fly (by building without prefixing, using
`read_symbols.go` to construct a symbol list, and then building again with
prefixing).

This mechanism is under development and may change over time. Please contact the
BoringSSL maintainers if making use of it.

## Known Limitations on Windows

  * Versions of CMake since 3.0.2 have a bug in its Ninja generator that causes
    yasm to output warnings

        yasm: warning: can open only one input file, only the last file will be processed

    These warnings can be safely ignored. The cmake bug is
    http://www.cmake.org/Bug/view.php?id=15253.

  * CMake can generate Visual Studio projects, but the generated project files
    don't have steps for assembling the assembly language source files, so they
    currently cannot be used to build BoringSSL.

## Embedded ARM

ARM, unlike Intel, does not have an instruction that allows applications to
discover the capabilities of the processor. Instead, the capability information
has to be provided by the operating system somehow.

By default, on Linux-based systems, BoringSSL will try to use `getauxval` and
`/proc` to discover the capabilities. But some environments don't support that
sort of thing and, for them, it's possible to configure the CPU capabilities at
compile time.

On iOS or builds which define `OPENSSL_STATIC_ARMCAP`, features will be
determined based on the `__ARM_NEON__` and `__ARM_FEATURE_CRYPTO` preprocessor
symbols reported by the compiler. These values are usually controlled by the
`-march` flag. You can also define any of the following to enable the
corresponding ARM feature.

  * `OPENSSL_STATIC_ARMCAP_NEON`
  * `OPENSSL_STATIC_ARMCAP_AES`
  * `OPENSSL_STATIC_ARMCAP_SHA1`
  * `OPENSSL_STATIC_ARMCAP_SHA256`
  * `OPENSSL_STATIC_ARMCAP_PMULL`

Note that if a feature is enabled in this way, but not actually supported at
run-time, BoringSSL will likely crash.

## Binary Size

The implementations of some algorithms require a trade-off between binary size
and performance. For instance, BoringSSL's fastest P-256 implementation uses a
148 KiB pre-computed table. To optimize instead for binary size, pass
`-DOPENSSL_SMALL=1` to CMake or define the `OPENSSL_SMALL` preprocessor symbol.

# Running Tests

There are two sets of tests: the C/C++ tests and the blackbox tests. For former
are built by Ninja and can be run from the top-level directory with `go run
util/all_tests.go`. The latter have to be run separately by running `go test`
from within `ssl/test/runner`.

Both sets of tests may also be run with `ninja -C build run_tests`, but CMake
3.2 or later is required to avoid Ninja's output buffering.
