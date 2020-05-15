# ANGLE for Android

Building ANGLE for Android is heavily dependent on the Chromium toolchain. It is not currently possible to build ANGLE for Android without a Chromium checkout. See http://anglebug.com/2344 for more details on why.

From a Linux platform (the only platform that Chromium for Android supports), follow the steps in
[Checking out and building Chromium for Android](https://chromium.googlesource.com/chromium/src/+/master/docs/android_build_instructions.md).

Name your output directories `out/Debug` and `out/Release`, because Chromium GPU tests look for browser binaries in these folders. Replacing '[Debug|Release]' with other names seems to be OK when working with multiple build configurations.

The following GN args are known to work well for Release and Debug configurations.

Debug:
```
$ gn args out/Debug
```
```
target_os = "android"
target_cpu = "arm64"
android32_ndk_api_level = 26
android64_ndk_api_level = 26
angle_libs_suffix = "_angle"
ffmpeg_branding = "Chrome"
is_component_build = false
symbol_level = 1
is_debug = true
dcheck_always_on = true
angle_enable_vulkan = true
angle_enable_vulkan_validation_layers = true
angle_enable_gl = false
```
Release:
```
$ gn args out/Release
```
```
target_os = "android"
target_cpu = "arm64"
android32_ndk_api_level = 26
android64_ndk_api_level = 26
angle_libs_suffix = "_angle"
ffmpeg_branding = "Chrome"
is_component_build = false
symbol_level = 0
strip_debug_info = true
is_debug = false
is_official_build = true # enables level of optimization beyond release
dcheck_always_on = false
angle_enable_vulkan = true
angle_enable_vulkan_validation_layers = false
angle_enable_gl = false
enable_resource_whitelist_generation = false
```

If you run into any problems with the above, you may be able to pull newer GN args from an official Android bot on [GPU.FYI waterfall](https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console).
 - Look for `generate_build_files` step output of that bot.
 - Remove `goma_dir` flag.

Build an ANGLE target using the following command:

```
autoninja -C out/Debug <target>
```
or
```
autoninja -C out/Release <target>
```

The following ANGLE build targets are supported:

```
angle
angle_apks
angle_deqp_gles2_tests
angle_deqp_khr_gles2_tests
angle_deqp_gles3_tests
angle_deqp_khr_gles3_tests
angle_deqp_gles31_tests
angle_deqp_khr_gles31_tests
angle_deqp_egl_tests
angle_end2end_tests
angle_perftests
angle_unittests
angle_white_box_perftests
angle_white_box_tests
```
ANGLE tests will be in your out/<target> directory, and can be run with various options. For instance, angle perftests can be run with:
```
./out/Debug/angle_perftests --use-angle=vulkan --verbose --local-output --gtest_filter="*foo*"
```

Additional details are in [Android Test Instructions](https://chromium.googlesource.com/chromium/src/+/master/docs/testing/android_test_instructions.md).

Additional Android dEQP notes can be found in [Running dEQP on Android](dEQP.md#Running-dEQP-on-Android).

If you are targeting WebGL and want to run GPU telemetry tests, build `chrome_public_apk` target. Then follow [GPU Testing](http://www.chromium.org/developers/testing/gpu-testing#TOC-Running-the-GPU-Tests-Locally) doc, using `--browser=android-chromium` argument. Make sure to set your `CHROMIUM_OUT_DIR` environment variable, so that your browser is found, otherwise the stock one will run. Also, follow [How to build ANGLE in Chromium for dev](BuildingAngleForChromiumDevelopment.md) to work with Top of Tree ANGLE in Chromium.

## Using ANGLE as the Android OpenGL ES driver

Starting with Android 10 (Q), ANGLE can be loaded as the OpenGL ES driver.

`== Important Note ==` ANGLE built this way can only be used for *DEBUGGABLE APPS* (i.e. [marked debuggable](https://developer.android.com/guide/topics/manifest/application-element#debug) in the manifest) or users with *ROOT ACCESS* (i.e. a [userdebug](https://source.android.com/setup/build/building) build).

To build the ANGLE APK, you must first bootstrap your build by following the steps [above](#ANGLE-for-Android). The steps below will result in an APK that contains the ANGLE libraries and can be installed on any Android 10+ build.

Apps can be opted in to ANGLE [one at a time](#ANGLE-for-a-single-OpenGL-ES-app), in [groups](#ANGLE-for-multiple-OpenGL-ES-apps), or [globally](#ANGLE-for-all-OpenGL-ES-apps), but they must be launched by the Java runtime since the libraries are discovered within an installed package. This means ANGLE cannot be used by native executables or SurfaceFlinger at this time.

## Build the ANGLE APK

Using 'gn args` from above, you can build the ANGLE apk using:
```
autoninja -C out/Debug angle_apks
```
## Install the ANGLE APK
```
adb install out/Debug/apks/AngleLibraries.apk
```
You can verify installation by looking for the package name:
```
$ adb shell pm path com.chromium.angle
package:/data/app/com.chromium.angle-HpkUceNFjoLYKPbIVxFWLQ==/base.apk
```
## Selecting ANGLE as the OpenGL ES driver

For debuggable applications or root users, you can tell the platform to load ANGLE libraries from the installed package.
```
adb shell settings put global angle_debug_package com.chromium.angle
```
Remember that ANGLE can only be used by applications launched by the Java runtime.

## ANGLE driver choices
There are multiple values you can use for selecting which OpenGL ES driver is loaded by the platform.

The following values are supported for `angle_gl_driver_selection_values`:
 - `angle` : Use ANGLE.
 - `native` : Use the native OpenGL ES driver.
 - `default` : Use the default driver. This allows the platform to decide which driver to use.

In each section below, replace `<driver>` with one of the values above.

## ANGLE for a *single* OpenGL ES app
```
adb shell settings put global angle_gl_driver_selection_pkgs <package name>
adb shell settings put global angle_gl_driver_selection_values <driver>
```
## ANGLE for *multiple* OpenGL ES apps
Similar to selecting a single app, you can select multiple applications by listing their package names and driver choice in comma separated lists.  Note the lists must be the same length, one driver choice per package name.
```
adb shell settings put global angle_gl_driver_selection_pkgs <package name 1>,<package name 2>,<package name 3>,...
adb shell settings put global angle_gl_driver_selection_values <driver 1>,<driver 2>,<driver 3>,...
```
## ANGLE for *all* OpenGL ES apps
Enable:
```
adb shell settings put global angle_gl_driver_all_angle 1
```
Disable:
```
adb shell settings put global angle_gl_driver_all_angle 0
```
## Check for success
Check to see that ANGLE was loaded by your application:
```
$ adb logcat -d | grep ANGLE
V GraphicsEnvironment: ANGLE developer option for <package name>: angle
I GraphicsEnvironment: ANGLE package enabled: com.chromium.angle
I ANGLE   : Version (2.1.0.f87fac56d22f), Renderer (Vulkan 1.1.87(Adreno (TM) 615 (0x06010501)))
```
## Clean up
Settings persist across reboots, so it is a good idea to delete them when finished.
```
adb shell settings delete global angle_debug_package
adb shell settings delete global angle_gl_driver_all_angle
adb shell settings delete global angle_gl_driver_selection_pkgs
adb shell settings delete global angle_gl_driver_selection_values
```
## Troubleshooting
If your application is not debuggable or you are not root, you may see an error like this in the log:
```
$ adb logcat -d | grep ANGLE
V GraphicsEnvironment: ANGLE developer option for <package name>: angle
E GraphicsEnvironment: Invalid number of ANGLE packages. Required: 1, Found: 0
E GraphicsEnvironment: Failed to find ANGLE package.
```
Double check that you are root, or that your application is [marked debuggable](https://developer.android.com/guide/topics/manifest/application-element#debug).
