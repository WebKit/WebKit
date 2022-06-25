# WebRTC Android development

## Getting the Code

Android development is only supported on Linux.

1. Install [prerequisite software][webrtc-prerequisite-sw]

2. Create a working directory, enter it, and run:

```
$ fetch --nohooks webrtc_android
$ gclient sync
```

This will fetch a regular WebRTC checkout with the Android-specific parts
added. Notice that the Android specific parts like the Android SDK and NDK are
quite large (~8 GB), so the total checkout size will be about 16 GB.
The same checkout can be used for both Linux and Android development since you
can generate your [Ninja][ninja] project files in different directories for each
build config.

See [Development][webrtc-development] for instructions on how to update
the code, building etc.


## Compiling

1. Generate projects using GN.

Make sure your current working directory is src/ of your workspace.
Then run:

```
$ gn gen out/Debug --args='target_os="android" target_cpu="arm"'
```

You can specify a directory of your own choice instead of `out/Debug`,
to enable managing multiple configurations in parallel.

* To build for ARM64: use `target_cpu="arm64"`
* To build for 32-bit x86: use `target_cpu="x86"`
* To build for 64-bit x64: use `target_cpu="x64"`

2. Compile using:

```
$ autoninja -C out/Debug
```

(To list all available targets, run `autoninja -C out/Debug -t targets all`.)


## Using the Bundled Android SDK/NDK

In order to use the Android SDK and NDK that is bundled in
`third_party/android_tools`, run this to get it included in your `PATH` (from
`src/`):

```
$ . build/android/envsetup.sh
```

Then you'll have `adb` and all the other Android tools in your `PATH`.


## Running the AppRTCMobile App

AppRTCMobile is an Android application using WebRTC Native APIs via JNI (JNI
wrapper is documented [here][webrtc-jni-doc]).

For instructions on how to build and run, see
[examples/androidapp/README][apprtc-doc].


## Using Android Studio

*Note: This is known to be broken at the moment. See bug:
https://bugs.webrtc.org/9282*

1. Build the project normally (out/Debug should be the directory you used when
generating the build files using GN):

```
$ autoninja -C out/Debug AppRTCMobile
```

2. Generate the project files:

```
$ build/android/gradle/generate_gradle.py --output-directory $PWD/out/Debug \
  --target "//examples:AppRTCMobile" --use-gradle-process-resources \
  --split-projects --canary
```

3. *Import* the project in Android Studio. (Do not just open it.) The project
is located in `out/Debug/gradle`. If asked which SDK to use, choose to use
Android Studio's SDK. When asked whether to use the Gradle wrapper, press
"OK".

4. Ensure target `webrtc > examples > AppRTCMobile` is selected and press Run.
AppRTCMobile should now start on the device.

If you do any changes to the C++ code, you have to compile the project using
autoninja after the changes (see step 1).

*Note: Only "arm" is supported as the target_cpu when using Android Studio. This
still allows you to run the application on 64-bit ARM devices. x86-based devices
are not supported right now.*


## Running Tests on an Android Device

To build APKs with the WebRTC native tests, follow these instructions.

1. Ensure you have an Android device set in Developer mode connected via USB.

2. Compile unit tests and/or instrumentation tests:

```
$ autoninja -C out/Debug android_instrumentation_test_apk
$ autoninja -C out/Debug rtc_unittests
```

3. You can find the generated test binaries in `out/Debug/bin`. To run instrumentation tests:

```
$ out/Debug/bin/run_android_instrumentation_test_apk -v
```

To run unit tests:

```
$ out/Debug/bin/run_rtc_unittests -v
```

Show verbose output with `-v` and filter tests with `--gtest-filter=SomeTest.*`. For example:

```
$ out/Debug/bin/run_android_instrumentation_test_apk -v \
    --gtest_filter=VideoFrameBufferTest.*
```

For a full list of command line arguments, use `--help`.

5. **NOTICE:** The first time you run a test, you must accept a dialog on
the device!

If want to run Release builds instead; pass `is_debug=false` to GN (and
preferably generate the projects files into a directory like `out/Release`).
Then use the scripts generated in `out/Release/bin` instead.

[webrtc-prerequisite-sw]: https://webrtc.googlesource.com/src/+/main/docs/native-code/development/prerequisite-sw/index.md
[webrtc-jni-doc]: https://webrtc.googlesource.com/src/+/main/sdk/android/README
[apprtc-doc]: https://webrtc.googlesource.com/src/+/main/examples/androidapp/README
[ninja]: https://ninja-build.org/
[prebuilt-libraries]: https://bintray.com/google/webrtc/google-webrtc
[webrtc-development]: https://webrtc.googlesource.com/src/+/main/docs/native-code/development/index.md
