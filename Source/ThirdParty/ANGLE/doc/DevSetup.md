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
 * Install [Visual Studio Community 2019](https://visualstudio.microsoft.com/vs/)
 * Install the [Windows 10 SDK, latest version](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk).
   * You can install it through Visual Studio Installer if available.
   * Required for GN-generated Visual Studio projects, the Debug runtime for D3D11, and the D3D Compiler DLL.
 * (optional) See the [Chromium Windows build instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/windows_build_instructions.md) for more info.

On Linux:

 * Dependencies will be handled later (see `install-build-deps.sh` below).

On MacOS:

 * [XCode](https://developer.apple.com/xcode/) for Clang and development files.
 * For Googlers on MacOS, you'll first need authorization to download macOS SDK's from Chromium
   servers before running `gclient sync`. Obtain this authorization via `cipd auth-login`
   and following the instructions.

### Getting the source

```
git clone https://chromium.googlesource.com/angle/angle
cd angle
python scripts/bootstrap.py
gclient sync
git checkout main
```

If you're contributing code, you will also need to set up the commit-msg hook. See [ContributingCode#getting-started-with-gerrit](ContributingCode.md#getting-started-with-gerrit) for more detailed instructions.

On Linux only, you need to install all the necessary dependencies before going further by running this command:
```
./build/install-build-deps.sh
```

After this completes successfully, you are ready to generate the ninja files:
```
gn gen out/Debug
```

On Windows only, ensure you **set `DEPOT_TOOLS_WIN_TOOLCHAIN=0` in your environment** (if you are not a Googler).

GN will generate ninja files. The default build options build ANGLE with clang and in release mode.
Often, the default options are the desired ones, but they can be changed by running
`gn args out/Debug`. Some options that are commonly overriden for development are:

```
is_component_build = false         (links dependencies into the build targets)
target_cpu = "x86"                 (default is "x64")
is_clang = false (NOT RECOMMENDED) (to use system default compiler instead of clang)
is_debug = false                   (for release builds. is_debug = true is the default)
angle_assert_always_on = true      (enable release asserts and debug layers)
```

For a release build run `gn args out/Release` and set `is_debug = false`.

On Windows, you can build for the Universal Windows Platform (UWP) by setting `target_os = "winuwp"` in the args.
Setting `is_component_build = false` is highly recommended to support moving libEGL.dll and libGLESv2.dll to an
application's directory and being self-contained, instead of depending on other DLLs (d3dcompiler_47.dll is still
needed for the Direct3D backend).

For more information on GN run `gn help`.

Ninja can be used to compile on all platforms with one of the following commands:
```
autoninja -C out/Debug
autoninja -C out/Release
```
Ninja automatically calls GN to regenerate the build files on any configuration change.

Ensure `depot_tools` is in your path as it provides ninja.

### Building with Goma (Google employees only)

In addition, Google employees should use goma, a distributed compilation
system. Detailed information is available internally but the relevant gn arg
is:

```
use_goma = true
```

To get any benefit from goma it is important to pass a large -j value to
ninja. A good default is 10*numCores to 20*numCores. If you run autoninja then
it will automatically pass an appropriate -j value to ninja for goma or not.

```
$ autoninja -C out\Debug
```

### Building and Debugging with Visual Studio

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

See the Android specific [documentation](DevSetupAndroid.md#ANGLE-for-Android).

## Application Development with ANGLE
This sections describes how to use ANGLE to build an OpenGL ES application.

### Choosing a Backend
ANGLE can use a variety of backing renderers based on platform.  On Windows, it defaults to D3D11 where it's available,
or D3D9 otherwise.  On other desktop platforms, it defaults to GL.  On mobile, it defaults to GLES.

ANGLE provides an EGL extension called `EGL_ANGLE_platform_angle` which allows uers to select
which renderer to use at EGL initialization time by calling eglGetPlatformDisplayEXT with special
enums. Details of the extension can be found in its specification in
`extensions/EGL_ANGLE_platform_angle.txt` and `extensions/EGL_ANGLE_platform_angle_*.txt` and
examples of its use can be seen in the ANGLE samples and tests, particularly `util/EGLWindow.cpp`.

To change the default D3D backend:

 1. Open `src/libANGLE/renderer/d3d/DisplayD3D.cpp`
 2. Locate the definition of `ANGLE_DEFAULT_D3D11` near the head of the file, and set it to your preference.

To remove any backend entirely:

 1. Run `gn args <path/to/build/dir>`
 2. Set the appropriate variable to `false`. Options are:
   - `angle_enable_d3d9`
   - `angle_enable_d3d11`
   - `angle_enable_gl`
   - `angle_enable_metal`
   - `angle_enable_null`
   - `angle_enable_vulkan`
   - `angle_enable_essl`
   - `angle_enable_glsl`

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

## GLSL ES Translator

In addition to OpenGL ES and EGL libraries, ANGLE also provides a GLSL ES
translator. The translator targets various back-ends, including HLSL, GLSL
for desktop and mobile, SPIR-V and Metal SL. To build the translator, build
the `angle_shader_translator` target. Run the translator binary without
arguments to see a usage message.

### Source and Building

The translator code is included with ANGLE but fully independent; it resides
in [`src/compiler`](../src/compiler). Follow the steps above for
[getting and building ANGLE](#getting-the-source) to build the translator on
the platform of your choice.

### Usage

The ANGLE [`shader_translator`](../samples/shader_translator/shader_translator.cpp)
sample demos basic C++ API usage. To translate a GLSL ES shader, call the following
functions in the same order:

 * `sh::Initialize()` initializes the translator library and must be called only once from each process using the translator.
 * `sh::ContructCompiler()` creates a translator object for vertex or fragment shader.
 * `sh::Compile()` translates the given shader.
 * `sh::Destruct()` destroys the given translator.
 * `sh::Finalize()` shuts down the translator library and must be called only once from each process using the translator.
