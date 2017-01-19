# Getting Started

How to get and build the libyuv code.

## Pre-requisites

You'll need to have depot tools installed: https://www.chromium.org/developers/how-tos/install-depot-tools
Refer to chromium instructions for each platform for other prerequisites.

## Getting the Code

Create a working directory, enter it, and run:

    gclient config https://chromium.googlesource.com/libyuv/libyuv
    gclient sync

Then you'll get a .gclient file like:

    solutions = [
      { "name"        : "libyuv",
        "url"         : "https://chromium.googlesource.com/libyuv/libyuv",
        "deps_file"   : "DEPS",
        "managed"     : True,
        "custom_deps" : {
        },
        "safesync_url": "",
      },
    ];

For iOS add `;target_os=['ios'];` to your OSX .gclient and run `GYP_DEFINES="OS=ios" gclient sync.`

Browse the Git reprository: https://chromium.googlesource.com/libyuv/libyuv/+/master

### Android
For Android add `;target_os=['android'];` to your Linux .gclient

    solutions = [
      { "name"        : "libyuv",
        "url"         : "https://chromium.googlesource.com/libyuv/libyuv",
        "deps_file"   : "DEPS",
        "managed"     : True,
        "custom_deps" : {
        },
        "safesync_url": "",
      },
    ];
    target_os = ["android", "unix"];

Then run:

    export GYP_DEFINES="OS=android"
    gclient sync

Caveat: Theres an error with Google Play services updates.  If you get the error "Your version of the Google Play services library is not up to date", run the following:

    cd chromium/src
    ./build/android/play_services/update.py download
    cd ../..

For Windows the gclient sync must be done from an Administrator command prompt.

The sync will generate native build files for your environment using gyp (Windows: Visual Studio, OSX: XCode, Linux: make). This generation can also be forced manually: `gclient runhooks`

To get just the source (not buildable):

    git clone https://chromium.googlesource.com/libyuv/libyuv


## Building the Library and Unittests

### Windows

    call gn gen out/Release "--args=is_debug=false target_cpu=\"x86\""
    call gn gen out/Debug "--args=is_debug=true target_cpu=\"x86\""
    ninja -j7 -v -C out/Release
    ninja -j7 -v -C out/Debug

    call gn gen out/Release "--args=is_debug=false target_cpu=\"x64\""
    call gn gen out/Debug "--args=is_debug=true target_cpu=\"x64\""
    ninja -j7 -v -C out/Release
    ninja -j7 -v -C out/Debug

#### Building with clang-cl

    set GYP_DEFINES=clang=1 target_arch=ia32
    call python tools\clang\scripts\update.py

    call gn gen out/Release "--args=is_debug=false is_official_build=false is_clang=true target_cpu=\"x86\""
    call gn gen out/Debug "--args=is_debug=true is_official_build=false is_clang=true target_cpu=\"x86\""
    ninja -j7 -v -C out/Release
    ninja -j7 -v -C out/Debug

    call gn gen out/Release "--args=is_debug=false is_official_build=false is_clang=true target_cpu=\"x64\""
    call gn gen out/Debug "--args=is_debug=true is_official_build=false is_clang=true target_cpu=\"x64\""
    ninja -j7 -v -C out/Release
    ninja -j7 -v -C out/Debug

### macOS and Linux

    gn gen out/Release "--args=is_debug=false"
    gn gen out/Debug "--args=is_debug=true"
    ninja -j7 -v -C out/Release
    ninja -j7 -v -C out/Debug

### Building Offical with GN

    gn gen out/Official "--args=is_debug=false is_official_build=true is_chrome_branded=true"
    ninja -C out/Official

### iOS
This section needs an update to GN build documentation.
http://www.chromium.org/developers/how-tos/build-instructions-ios

Add to .gclient last line: `target_os=['ios'];`

armv7

    GYP_DEFINES="OS=ios target_arch=armv7 target_subarch=arm32" GYP_CROSSCOMPILE=1 GYP_GENERATOR_FLAGS="output_dir=out_ios" ./gyp_libyuv
    ninja -j7 -C out_ios/Debug-iphoneos libyuv_unittest
    ninja -j7 -C out_ios/Release-iphoneos libyuv_unittest

arm64

    GYP_DEFINES="OS=ios target_arch=arm64 target_subarch=arm64" GYP_CROSSCOMPILE=1 GYP_GENERATOR_FLAGS="output_dir=out_ios" ./gyp_libyuv
    ninja -j7 -C out_ios/Debug-iphoneos libyuv_unittest
    ninja -j7 -C out_ios/Release-iphoneos libyuv_unittest

both armv7 and arm64 (fat)

    GYP_DEFINES="OS=ios target_arch=armv7 target_subarch=both" GYP_CROSSCOMPILE=1 GYP_GENERATOR_FLAGS="output_dir=out_ios" ./gyp_libyuv
    ninja -j7 -C out_ios/Debug-iphoneos libyuv_unittest
    ninja -j7 -C out_ios/Release-iphoneos libyuv_unittest

simulator

    GYP_DEFINES="OS=ios target_arch=ia32 target_subarch=arm32" GYP_CROSSCOMPILE=1 GYP_GENERATOR_FLAGS="output_dir=out_sim" ./gyp_libyuv
    ninja -j7 -C out_sim/Debug-iphonesimulator libyuv_unittest
    ninja -j7 -C out_sim/Release-iphonesimulator libyuv_unittest

### Android
https://code.google.com/p/chromium/wiki/AndroidBuildInstructions

Add to .gclient last line: `target_os=['android'];`

armv7

    gn gen out/Release "--args=is_debug=false target_os=\"android\" target_cpu=\"arm\""
    gn gen out/Debug "--args=is_debug=true target_os=\"android\" target_cpu=\"arm\""
    ninja -j7 -v -C out/Debug libyuv_unittest
    ninja -j7 -v -C out/Release libyuv_unittest

arm64

    gn gen out/Release "--args=is_debug=false target_os=\"android\" target_cpu=\"arm64\""
    gn gen out/Debug "--args=is_debug=true target_os=\"android\" target_cpu=\"arm64\""
    ninja -j7 -v -C out/Debug libyuv_unittest
    ninja -j7 -v -C out/Release libyuv_unittest

ia32

    gn gen out/Release "--args=is_debug=false target_os=\"android\" target_cpu=\"x86\""
    gn gen out/Debug "--args=is_debug=true target_os=\"android\" target_cpu=\"x86\""
    ninja -j7 -v -C out/Debug libyuv_unittest
    ninja -j7 -v -C out/Release libyuv_unittest

mipsel

    gn gen out/Release "--args=is_debug=false target_os=\"android\" target_cpu=\"mipsel\" mips_arch_variant=\"r6\" mips_use_msa=true is_component_build=true is_clang=false"
    gn gen out/Debug "--args=is_debug=true target_os=\"android\" target_cpu=\"mipsel\" mips_arch_variant=\"r6\" mips_use_msa=true is_component_build=true is_clang=false"
    ninja -j7 -v -C out/Debug libyuv_unittest
    ninja -j7 -v -C out/Release libyuv_unittest

    gn gen out/Release "--args=is_debug=false target_os=\"android\" target_cpu=\"mips64el\" mips_arch_variant=\"r6\" mips_use_msa=true is_component_build=true is_clang=false"
    gn gen out/Debug "--args=is_debug=true target_os=\"android\" target_cpu=\"mips64el\" mips_arch_variant=\"r6\" mips_use_msa=true is_component_build=true is_clang=false"
    ninja -j7 -v -C out/Debug libyuv_unittest
    ninja -j7 -v -C out/Release libyuv_unittest

arm disassembly:

    third_party/android_tools/ndk/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-objdump -d ./out/Release/obj/libyuv/row_common.o >row_common.txt

    third_party/android_tools/ndk/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-objdump -d ./out/Release/obj/libyuv_neon/row_neon.o >row_neon.txt

    third_party/android_tools/ndk/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-objdump -d ./out/Release/obj/libyuv_neon/row_neon64.o >row_neon64.txt

Running tests:

    util/android/test_runner.py gtest -s libyuv_unittest -t 7200 --verbose --release --gtest_filter=*

Running test as benchmark:

    util/android/test_runner.py gtest -s libyuv_unittest -t 7200 --verbose --release --gtest_filter=* -a "--libyuv_width=1280 --libyuv_height=720 --libyuv_repeat=999 --libyuv_flags=-1  --libyuv_cpu_info=-1"

Running test with C code:

    util/android/test_runner.py gtest -s libyuv_unittest -t 7200 --verbose --release --gtest_filter=* -a "--libyuv_width=1280 --libyuv_height=720 --libyuv_repeat=999 --libyuv_flags=1 --libyuv_cpu_info=1"

### Build targets

    ninja -C out/Debug libyuv
    ninja -C out/Debug libyuv_unittest
    ninja -C out/Debug compare
    ninja -C out/Debug convert
    ninja -C out/Debug psnr
    ninja -C out/Debug cpuid

### ARM Linux

    gn gen out/Release "--args=is_debug=false target_cpu=\"arm64\""
    gn gen out/Debug "--args=is_debug=true target_cpu=\"arm64\""
    ninja -j7 -v -C out/Debug libyuv_unittest
    ninja -j7 -v -C out/Release libyuv_unittest

## Building the Library with make

### Linux

    make -j7 V=1 -f linux.mk
    make -j7 V=1 -f linux.mk clean
    make -j7 V=1 -f linux.mk CXX=clang++

## Building the Library with cmake

Install cmake: http://www.cmake.org/

Default debug build:

    mkdir out
    cd out
    cmake ..
    cmake --build .

Release build/install

    mkdir out
    cd out
    cmake -DCMAKE_INSTALL_PREFIX="/usr/lib" -DCMAKE_BUILD_TYPE="Release" ..
    cmake --build . --config Release
    sudo cmake --build . --target install --config Release

## Running Unittests

### Windows

    out\Release\libyuv_unittest.exe --gtest_catch_exceptions=0 --gtest_filter="*"

### OSX

    out/Release/libyuv_unittest --gtest_filter="*"

### Linux

    out/Release/libyuv_unittest --gtest_filter="*"

Replace --gtest_filter="*" with specific unittest to run.  May include wildcards. e.g.

    out/Release/libyuv_unittest --gtest_filter=*I420ToARGB_Opt

## CPU Emulator tools

### Intel SDE (Software Development Emulator)

Pre-requisite: Install IntelSDE: http://software.intel.com/en-us/articles/intel-software-development-emulator

Then run:

    c:\intelsde\sde -hsw -- out\Release\libyuv_unittest.exe --gtest_filter=*

    ~/intelsde/sde -skx -- out/Release/libyuv_unittest --gtest_filter=**I420ToARGB_Opt

## Sanitizers

    gn gen out/Debug "--args=is_debug=true is_asan=true"
    ninja -j7 -v -C out/Debug

    Sanitizers available: tsan, msan, asan, ubsan, lsan

### Running Dr Memory memcheck for Windows

Pre-requisite: Install Dr Memory for Windows and add it to your path: http://www.drmemory.org/docs/page_install_windows.html

    drmemory out\Debug\libyuv_unittest.exe --gtest_catch_exceptions=0 --gtest_filter=*
