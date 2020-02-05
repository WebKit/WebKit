# ANGLE Development

ANGLE provides OpenGL ES 3.1 and EGL 1.5 libraries and tests. You can use these to build and run OpenGL ES applications on Windows, Linux, Mac and Android.

## Development setup

### Version Control
ANGLE uses git for version control. Helpful documentation can be found at [http://git-scm.com/documentation](http://git-scm.com/documentation).

### Required Tools
On all platforms:

 * [depot_tools](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up)
   * Required to download dependencies (with gclient), generate build files (with GN), and compile ANGLE (with ninja).
   * Ensure `depot_tools` is in your path as it provides ninja for compilation.
 * For Googlers, run `download_from_google_storage --config` to login to Google Storage.

On Windows:

 * ***IMPORTANT: Set `DEPOT_TOOLS_WIN_TOOLCHAIN=0` in your environment if you are not a Googler.***
 * [Visual Studio Community 2019](https://visualstudio.microsoft.com/vs/)
 * [Windows 10 Standalone SDK version 10.0.17134 exactly](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk).
   * You should install it through Visual Studio Installer if available.
   * Comes with additional features that aid development, such as the Debug runtime for D3D11. Required for the D3D Compiler DLL.
 * (optional) See the [Chromium Windows build instructions](https://chromium.googlesource.com/chromium/src/+/master/docs/windows_build_instructions.md) for more info.

On Linux:

 * Install package dependencies by running `install-build-deps.sh` later on.
 * Bison and flex are not needed as we only support generating the translator grammar on Windows.

On MacOS:

 * [XCode](https://developer.apple.com/xcode/) for Clang and development files.
 * Bison and flex are not needed as we only support generating the translator grammar on Windows.

### Getting the source

```
git clone https://chromium.googlesource.com/angle/angle
cd angle
python scripts/bootstrap.py
gclient sync
git checkout master
```

On Linux only, you need to install all the necessary dependencies before going further by running this command:
```
./build/install-build-deps.sh
```

After this completes successfully, you are ready to generate the ninja files:
```
gn gen out/Debug
```

On Windows only, ensure you **set `DEPOT_TOOLS_WIN_TOOLCHAIN=0` in your environment** (if you are not a Googler).

GN will generate ninja files. To change the default build options run `gn args out/Debug`.  Some commonly used options are:
```
target_cpu = "x86"        (default is "x64")
is_clang = false          (to use system default compiler instead of clang)
is_debug = true           (enable debugging, true is the default)
dcheck_always_on = true   (enable release asserts and debug layers)
```

For a release build run `gn args out/Release` and set `is_debug = false`.

On Windows, you can build for the Universal Windows Platform (UWP) by setting `target_os = "winuwp"` in the args.

For more information on GN run `gn help`.

Ninja can be used to compile on all platforms with one of the following commands:
```
autoninja -C out/Debug
autoninja -C out/Release
```
Ninja automatically calls GN to regenerate the build files on any configuration change.

Ensure `depot_tools` is in your path as it provides ninja.

### Building with Visual Studio

To generate the Visual Studio solution in `out/Debug/angle-debug.sln`:
```
gn gen out/Debug --sln=angle-debug --ide=vs2019
```

In Visual Studio:
 1. Open the ANGLE solution file `out/Debug/angle-debug.sln`.
 2. It is recommended you still use `autoninja` from the command line to build.
 3. If you do want to build in the solution, "Build Solution" is not functional with GN. Build one target at a time.

Once the build completes all ANGLE libraries, tests, and samples will be located in `out/Debug`.

### Building ANGLE for Android

Building ANGLE for Android is heavily dependent on the Chromium toolchain. It is not currently possible to build ANGLE for Android without a Chromium checkout. See http://anglebug.com/2344 for more details on why.
Please follow the steps in
[Checking out and building Chromium for Android](https://chromium.googlesource.com/chromium/src/+/master/docs/android_build_instructions.md).
This must be done on Linux, the only platform that Chromium for Android supports.
Name your output directories `out/Debug` and `out/Release`, because Chromium GPU tests look for browser binaries in these folders. Replacing `out` with other names seems to be OK when working with multiple build configurations.
It's best to use a build configuration of some Android bot on [GPU.FYI waterfall](https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console). Look for `generate_build_files` step output of that bot. Remove `goma_dir` flag.
For example, these are the build flags from Nexus 5X bot:
```
build_angle_deqp_tests = true
dcheck_always_on = true
ffmpeg_branding = "Chrome"
is_component_build = false
is_debug = false
proprietary_codecs = true
symbol_level = 1
target_cpu = "arm64"          # Nexus 5X is 64 bit, remove this on 32 bit devices
target_os = "android"
use_goma = true               # Remove this if you don't have goma
```
Additional flags to build the Vulkan backend, enable only if running on Android O or higher:
```
android32_ndk_api_level = 26
android64_ndk_api_level = 26
```

These ANGLE targets are supported:
`ninja -C out/Release translator libEGL libGLESv2 angle_unittests angle_end2end_tests angle_white_box_tests angle_deqp_gles2_tests angle_deqp_gles3_tests angle_deqp_egl_tests angle_perftests angle_white_box_perftests`

In order to run ANGLE tests, prepend `bin/run_` to the test name, for example: `./out/Release/bin/run_angle_unittests`.
Additional details are in [Android Test Instructions](https://chromium.googlesource.com/chromium/src/+/master/docs/testing/android_test_instructions.md).

**dEQP Note**: Running the tests not using the test runner is tricky, but is necessary in order to get a complete TestResults.qpa from the dEQP tests (since the runner shards the tests, only the results of the last shard will be available when using the test runner). First, use the runner to install the APK, test data and test expectations on the device. After the tests start running, the test runner can be stopped with Ctrl+C. Then, run
```
adb shell am start -a android.intent.action.MAIN -n org.chromium.native_test/.NativeUnitTestNativeActivity -e org.chromium.native_test.NativeTest.StdoutFile /sdcard/chromium_tests_root/out.txt
```
After the tests finish, get the results with
```
adb pull /sdcard/chromium_tests_root/third_party/angle/third_party/deqp/src/data/TestResults.qpa .
```
Note: this location might change, one can double-check with `adb logcat -d | grep qpa`.

In order to run GPU telemetry tests, build `chrome_public_apk` target. Then follow [GPU Testing](http://www.chromium.org/developers/testing/gpu-testing#TOC-Running-the-GPU-Tests-Locally) doc, using `--browser=android-chromium` argument. Make sure to set your `CHROMIUM_OUT_DIR` environment variable, so that your browser is found, otherwise the stock one will run.

Also, follow [How to build ANGLE in Chromium for dev](BuildingAngleForChromiumDevelopment.md) to work with Top of Tree ANGLE in Chromium.

## Application Development with ANGLE
This sections describes how to use ANGLE to build an OpenGL ES application.

### Choosing a Backend
ANGLE can use a variety of backing renderers based on platform.  On Windows, it defaults to D3D11 where it's available,
or D3D9 otherwise.  On other desktop platforms, it defaults to GL.  On mobile, it defaults to GLES.

ANGLE provides an EGL extension called `EGL_ANGLE_platform_angle` which allows uers to select which renderer to use at EGL initialization time by calling eglGetPlatformDisplayEXT with special enums. Details of the extension can be found in it's specification in `extensions/ANGLE_platform_angle.txt` and `extensions/ANGLE_platform_angle_*.txt` and examples of it's use can be seen in the ANGLE samples and tests, particularly `util/EGLWindow.cpp`.

To change the default D3D backend:

 1. Open `src/libANGLE/renderer/d3d/DisplayD3D.cpp`
 2. Locate the definition of `ANGLE_DEFAULT_D3D11` near the head of the file, and set it to your preference.

### To Use ANGLE in Your Application
On Windows:

 1. Configure your build environment to have access to the `include` folder to provide access to the standard Khronos EGL and GLES2 header files.
  * For Visual C++
     * Right-click your project in the _Solution Explorer_, and select _Properties_.
     * Under the _Configuration Properties_ branch, click _C/C++_.
     * Add the relative path to the Khronos EGL and GLES2 header files to _Additional Include Directories_.
 2. Configure your build environment to have access to `libEGL.lib` and `libGLESv2.lib` found in the build output directory (see [Building ANGLE](#building-with-visual-studio)).
   * For Visual C++
     * Right-click your project in the _Solution Explorer_, and select _Properties_.
     * Under the _Configuration Properties_ branch, open the _Linker_ branch and click _Input_.
     * Add the relative paths to both the `libEGL.lib` file and `libGLESv2.lib` file to _Additional Dependencies_, separated by a semicolon.
 3. Copy `libEGL.dll` and `libGLESv2.dll` from the build output directory (see [Building ANGLE](#building-with-visual-studio)) into your application folder.
 4. Code your application to the Khronos [OpenGL ES 2.0](http://www.khronos.org/registry/gles/) and [EGL 1.4](http://www.khronos.org/registry/egl/) APIs.

On Linux and MacOS, either:

 - Link you application against `libGLESv2` and `libEGL`
 - Use `dlopen` to load the OpenGL ES and EGL entry points at runtime.

## GLSL ES to GLSL Translator
In addition to OpenGL ES 2.0 and EGL 1.4 libraries, ANGLE also provides a GLSL ES to GLSL translator. This is useful for implementing OpenGL ES emulators on top of desktop OpenGL.

### Source and Building
The translator code is included with ANGLE but fully independent; it resides in `src/compiler`.
Follow the steps above for [getting and building ANGLE](#getting-the-source) to build the translator on the platform of your choice.

### Usage
The basic usage is shown in `essl_to_glsl` sample under `samples/translator`. To translate a GLSL ES shader, following functions need to be called in the same order:

 * `ShInitialize()` initializes the translator library and must be called only once from each process using the translator.
 * `ShContructCompiler()` creates a translator object for vertex or fragment shader.
 * `ShCompile()` translates the given shader.
 * `ShDestruct()` destroys the given translator.
 * `ShFinalize()` shuts down the translator library and must be called only once from each process using the translator.
