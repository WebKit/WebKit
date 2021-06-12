# Deprecated Builds

Older documentation on build configs which are no longer supported.

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

    set GYP_DEFINES=target_arch=ia32
    call python gyp_libyuv -fninja -G msvs_version=2013
    ninja -j7 -C out\Release
    ninja -j7 -C out\Debug

    set GYP_DEFINES=target_arch=x64
    call python gyp_libyuv -fninja -G msvs_version=2013
    ninja -C out\Debug_x64
    ninja -C out\Release_x64

#### Building with clangcl
    set GYP_DEFINES=clang=1 target_arch=ia32
    call python tools\clang\scripts\update.py
    call python gyp_libyuv -fninja libyuv_test.gyp
    ninja -C out\Debug
    ninja -C out\Release

### OSX

Clang 64 bit shown. Remove `clang=1` for GCC and change x64 to ia32 for 32 bit.

    GYP_DEFINES="clang=1 target_arch=x64" ./gyp_libyuv
    ninja -j7 -C out/Debug
    ninja -j7 -C out/Release

    GYP_DEFINES="clang=1 target_arch=ia32" ./gyp_libyuv
    ninja -j7 -C out/Debug
    ninja -j7 -C out/Release

### iOS
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

    GYP_DEFINES="OS=android" GYP_CROSSCOMPILE=1 ./gyp_libyuv
    ninja -j7 -C out/Debug yuv_unittest_apk
    ninja -j7 -C out/Release yuv_unittest_apk

arm64

    GYP_DEFINES="OS=android target_arch=arm64 target_subarch=arm64" GYP_CROSSCOMPILE=1 ./gyp_libyuv
    ninja -j7 -C out/Debug yuv_unittest_apk
    ninja -j7 -C out/Release yuv_unittest_apk

ia32

    GYP_DEFINES="OS=android target_arch=ia32" GYP_CROSSCOMPILE=1 ./gyp_libyuv
    ninja -j7 -C out/Debug yuv_unittest_apk
    ninja -j7 -C out/Release yuv_unittest_apk

    GYP_DEFINES="OS=android target_arch=ia32 android_full_debug=1" GYP_CROSSCOMPILE=1 ./gyp_libyuv
    ninja -j7 -C out/Debug yuv_unittest_apk

mipsel

    GYP_DEFINES="OS=android target_arch=mipsel" GYP_CROSSCOMPILE=1 ./gyp_libyuv
    ninja -j7 -C out/Debug yuv_unittest_apk
    ninja -j7 -C out/Release yuv_unittest_apk

arm32 disassembly:

    third_party/android_ndk/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-objdump -d out/Release/obj/source/libyuv.row_neon.o

arm64 disassembly:

    third_party/android_ndk/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-objdump -d out/Release/obj/source/libyuv.row_neon64.o

Running tests:

    build/android/test_runner.py gtest -s libyuv_unittest -t 7200 --verbose --release --gtest_filter=*

Running test as benchmark:

    build/android/test_runner.py gtest -s libyuv_unittest -t 7200 --verbose --release --gtest_filter=* -a "--libyuv_width=1280 --libyuv_height=720 --libyuv_repeat=999 --libyuv_flags=-1"

Running test with C code:

    build/android/test_runner.py gtest -s libyuv_unittest -t 7200 --verbose --release --gtest_filter=* -a "--libyuv_width=1280 --libyuv_height=720 --libyuv_repeat=999 --libyuv_flags=1 --libyuv_cpu_info=1"

#### Building with GN

    gn gen out/Release "--args=is_debug=false target_cpu=\"x86\""
    gn gen out/Debug "--args=is_debug=true target_cpu=\"x86\""
    ninja -C out/Release
    ninja -C out/Debug

### Building Offical with GN

    gn gen out/Official "--args=is_debug=false is_official_build=true is_chrome_branded=true"
    ninja -C out/Official

#### Building mips with GN

mipsel
    gn gen out/Default "--args=is_debug=false target_cpu=\"mipsel\" target_os = \"android\" mips_arch_variant = \"r6\" mips_use_msa = true is_component_build = true is_clang = false"
    ninja -C out/Default

mips64el
    gn gen out/Default "--args=is_debug=false target_cpu=\"mips64el\" target_os = \"android\" mips_arch_variant = \"r6\" mips_use_msa = true is_component_build = true is_clang = false"
    ninja -C out/Default

### Linux

    GYP_DEFINES="target_arch=x64" ./gyp_libyuv
    ninja -j7 -C out/Debug
    ninja -j7 -C out/Release

    GYP_DEFINES="target_arch=ia32" ./gyp_libyuv
    ninja -j7 -C out/Debug
    ninja -j7 -C out/Release

#### CentOS

On CentOS 32 bit the following work around allows a sync:

    export GYP_DEFINES="host_arch=ia32"
    gclient sync

### Windows Shared Library

Modify libyuv.gyp from 'static_library' to 'shared_library', and add 'LIBYUV_BUILDING_SHARED_LIBRARY' to 'defines'.

    gclient runhooks

After this command follow the building the library instructions above.

If you get a compile error for atlthunk.lib on Windows, read http://www.chromium.org/developers/how-tos/build-instructions-windows


### Build targets

    ninja -C out/Debug libyuv
    ninja -C out/Debug libyuv_unittest
    ninja -C out/Debug compare
    ninja -C out/Debug yuvconvert
    ninja -C out/Debug yuvconstants
    ninja -C out/Debug psnr
    ninja -C out/Debug cpuid


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

### Windows 8 Phone

Pre-requisite:

* Install Visual Studio 2012 and Arm to your environment.<br>

Then:

    call "c:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\x86_arm\vcvarsx86_arm.bat"

or with Visual Studio 2013:

    call "c:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\x86_arm\vcvarsx86_arm.bat"
    nmake /f winarm.mk clean
    nmake /f winarm.mk

### Windows Shared Library

Modify libyuv.gyp from 'static_library' to 'shared_library', and add 'LIBYUV_BUILDING_SHARED_LIBRARY' to 'defines'. Then run this.

    gclient runhooks

After this command follow the building the library instructions above.

If you get a compile error for atlthunk.lib on Windows, read http://www.chromium.org/developers/how-tos/build-instructions-windows

### 64 bit Windows

    set GYP_DEFINES=target_arch=x64
    gclient runhooks V=1

### ARM Linux

    export GYP_DEFINES="target_arch=arm"
    export CROSSTOOL=`<path>`/arm-none-linux-gnueabi
    export CXX=$CROSSTOOL-g++
    export CC=$CROSSTOOL-gcc
    export AR=$CROSSTOOL-ar
    export AS=$CROSSTOOL-as
    export RANLIB=$CROSSTOOL-ranlib
    gclient runhooks

## Running Unittests

### Windows

    out\Release\libyuv_unittest.exe --gtest_catch_exceptions=0 --gtest_filter="*"

### OSX

    out/Release/libyuv_unittest --gtest_filter="*"

### Linux

    out/Release/libyuv_unittest --gtest_filter="*"

Replace --gtest_filter="*" with specific unittest to run.  May include wildcards. e.g.

    out/Release/libyuv_unittest --gtest_filter=libyuvTest.I420ToARGB_Opt

## CPU Emulator tools

### Intel SDE (Software Development Emulator)

Pre-requisite: Install IntelSDE for Windows: http://software.intel.com/en-us/articles/intel-software-development-emulator

Then run:

    c:\intelsde\sde -hsw -- out\release\libyuv_unittest.exe --gtest_filter=*


## Memory tools

### Running Dr Memory memcheck for Windows

Pre-requisite: Install Dr Memory for Windows and add it to your path: http://www.drmemory.org/docs/page_install_windows.html

    set GYP_DEFINES=build_for_tool=drmemory target_arch=ia32
    call python gyp_libyuv -fninja -G msvs_version=2013
    ninja -C out\Debug
    drmemory out\Debug\libyuv_unittest.exe --gtest_catch_exceptions=0 --gtest_filter=*

### Running UBSan

See Chromium instructions for sanitizers: https://www.chromium.org/developers/testing/undefinedbehaviorsanitizer

Sanitizers available: TSan, MSan, ASan, UBSan, LSan

    GYP_DEFINES='ubsan=1' gclient runhooks
    ninja -C out/Release

### Running Valgrind memcheck

Memory errors and race conditions can be found by running tests under special memory tools. [Valgrind] [1] is an instrumentation framework for building dynamic analysis tools. Various tests and profilers are built upon it to find memory handling errors and memory leaks, for instance.

[1]: http://valgrind.org

    solutions = [
      { "name"        : "libyuv",
        "url"         : "https://chromium.googlesource.com/libyuv/libyuv",
        "deps_file"   : "DEPS",
        "managed"     : True,
        "custom_deps" : {
           "libyuv/chromium/src/third_party/valgrind": "https://chromium.googlesource.com/chromium/deps/valgrind/binaries",
        },
        "safesync_url": "",
      },
    ]

Then run:

    GYP_DEFINES="clang=0 target_arch=x64 build_for_tool=memcheck" python gyp_libyuv
    ninja -C out/Debug
    valgrind out/Debug/libyuv_unittest


For more information, see http://www.chromium.org/developers/how-tos/using-valgrind

### Running Thread Sanitizer (TSan)

    GYP_DEFINES="clang=0 target_arch=x64 build_for_tool=tsan" python gyp_libyuv
    ninja -C out/Debug
    valgrind out/Debug/libyuv_unittest

For more info, see http://www.chromium.org/developers/how-tos/using-valgrind/threadsanitizer

### Running Address Sanitizer (ASan)

    GYP_DEFINES="clang=0 target_arch=x64 build_for_tool=asan" python gyp_libyuv
    ninja -C out/Debug
    valgrind out/Debug/libyuv_unittest

For more info, see http://dev.chromium.org/developers/testing/addresssanitizer

## Benchmarking

The unittests can be used to benchmark.

### Windows

    set LIBYUV_WIDTH=1280
    set LIBYUV_HEIGHT=720
    set LIBYUV_REPEAT=999
    set LIBYUV_FLAGS=-1
    out\Release\libyuv_unittest.exe --gtest_filter=*I420ToARGB_Opt


### Linux and Mac

    LIBYUV_WIDTH=1280 LIBYUV_HEIGHT=720 LIBYUV_REPEAT=1000 out/Release/libyuv_unittest --gtest_filter=*I420ToARGB_Opt

    libyuvTest.I420ToARGB_Opt (547 ms)

Indicates 0.547 ms/frame for 1280 x 720.

## Making a change

    gclient sync
    git checkout -b mycl -t origin/master
    git pull
    <edit files>
    git add -u
    git commit -m "my change"
    git cl lint
    git cl try
    git cl upload -r a-reviewer@chomium.org -s
    <once approved..>
    git cl land
