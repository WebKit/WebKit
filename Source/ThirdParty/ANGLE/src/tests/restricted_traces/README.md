# ANGLE Restricted Traces

The files in this directory are traces of real applications. We host them
internally because they may contain third party IP which we don't want
to share publicly.

## Accessing the traces

In order to compile and run with these, you must be granted access by Google,
then authenticate with [CIPD](CIPD). Googlers, use your @google account.
```
cipd auth-login
```
Add the following to ANGLE's .gclient file:
```
    "custom_vars": {
      "checkout_angle_restricted_traces": True
    },
```
Then use gclient to pull down binary files from CIPD.
```
gclient sync -D
```
This should result in a number of directories created in `src/tests/restricted_traces` that contain
the trace files listed in [restricted_traces.json](restricted_traces.json):
```
$ ls -d src/tests/restricted_traces/*/
src/tests/restricted_traces/aliexpress/
src/tests/restricted_traces/angry_birds_2_1500/
src/tests/restricted_traces/arena_of_valor/
src/tests/restricted_traces/asphalt_8/
src/tests/restricted_traces/brawl_stars/
src/tests/restricted_traces/bus_simulator_indonesia/
src/tests/restricted_traces/candy_crush_500/
src/tests/restricted_traces/clash_of_clans/
src/tests/restricted_traces/clash_royale/
src/tests/restricted_traces/cod_mobile/
...
```

[CIPD]: https://chromium.googlesource.com/infra/luci/luci-go/+/main/cipd/README.md

## Building the trace tests

To build for Android, follow the steps in [DevSetupAndroid.md](../../../doc/DevSetupAndroid.md)
(Recommend using the [`Performance`](../../../doc/DevSetupAndroid.md#performance-config) arguments
for best performance)

To build for Desktop, follow the steps in [DevSetup.md](../../../doc/DevSetup.md)

When that is working, add the following GN arg to your setup:
```
build_angle_trace_perf_tests = true
```
To build the angle_perftests:
```
autoninja -C out/<config> angle_perftests
```
Run them like so:
```
out/<config>/angle_perftests --gtest_filter=TracePerfTest*
```

# Capturing and adding new Android traces

Generally we want to use a Debug setup for recording new traces. That allows us to see asserts and
errors if the tracer needs to be improved.
Add the following GN arg to your Debug setup:
```
angle_with_capture_by_default = true
```

After [building](../../../doc/DevSetupAndroid.md#building-angle-for-android) and
[installing](../../../doc/DevSetupAndroid.md#install-the-angle-apk) the APK with the above arg,
we're ready to start capturing.

## Determine the target app

We first need to identify which application we want to trace.  That can generally be done by
looking at the web-based Play Store entry for your app.  For instance, Angry Birds 2 is listed
here: https://play.google.com/store/apps/details?id=com.rovio.baba

If there is no Play Store entry for your app, there are a couple of ways you can determine the
app's name.

If you have a userdebug build of Android, you can check logcat when you launch the application.
You should see an entry like this:
```
GraphicsEnvironment: ANGLE Developer option for 'com.rovio.baba' set to: 'default'
```
If you just have an APK, you can use the following command to find the package name:
```
$ aapt dump badging angry_birds_2.apk | grep package
package: name='com.rovio.baba' versionCode='24900001' versionName='2.49.1' platformBuildVersionName=''
```
You can also just guess at the package name, then check your device to see if it is installed. Keep
trying combinations until you find it:
```
$ adb shell pm list packages | grep rovio
package:com.rovio.baba
```
Track the package name for use in later steps:
```
export PACKAGE_NAME=com.rovio.baba
```

## Choose a trace name

Next, we need to chose a name for the trace. Choose something simple that identifies the app, then use snake
case. This will be the name of the trace files, including the trace directory. Changing this value later is possible,
but not recommended.
```
export LABEL=angry_birds_2
```

## Opt the application into ANGLE

Next, opt the application into using your ANGLE with capture enabled by default:
```
adb shell settings put global angle_debug_package org.chromium.angle
adb shell settings put global angle_gl_driver_selection_pkgs $PACKAGE_NAME
adb shell settings put global angle_gl_driver_selection_values angle
```

## Set up some Capture/Replay properties

We also need to set some debug properties used by the tracer.

Ensure frame capture is enabled. This might be redundant, but ensure the property isn't set to
zero, which disables frame capture.
```
adb shell setprop debug.angle.capture.enabled 1
```
Empty the start and end frames. Again, this might be redundant, but it is less confusing.
```
adb shell setprop debug.angle.capture.frame_start '""'
adb shell setprop debug.angle.capture.frame_end '""'
```
Set the label to be used in the trace files
```
adb shell setprop debug.angle.capture.label $LABEL
```
Set a trigger value to be used by the tracer. This should be set to the *number of frames* you want
to capture. We typically use 10 to get an idea of how a scene is running, but some workloads
require more. Use your discretion here:
```
adb shell setprop debug.angle.capture.trigger 10
```

## Create output location

We need to write out the trace file in a location accessible by the app. We use the app's data
storage on sdcard, but create a subfolder to isolate ANGLE's files:
```
adb shell mkdir -p /sdcard/Android/data/$PACKAGE_NAME/angle_capture
```

## Start the target app

From here, you can start the application. You should see logcat entries like the following,
indicating that we've succesfully turned on capturing:
```
ANGLE   : INFO: Capture trigger detected, disabling capture start/end frame.
ANGLE   : INFO: Limiting binary format support count to zero while FrameCapture enabled
ANGLE   : INFO: Limiting image unit count to 8 while FrameCapture enabled
ANGLE   : INFO: Setting uniform buffer offset alignment to 256 while FrameCapture enabled
ANGLE   : INFO: Disabling GL_EXT_map_buffer_range and GL_OES_mapbuffer during capture, which are not supported on some native drivers
ANGLE   : INFO: Disabling GL_CHROMIUM_bind_uniform_location during capture, which is not supported on native drivers
ANGLE   : INFO: Disabling GL_NV_shader_noperspective_interpolation during capture, which is not supported on some native drivers
ANGLE   : INFO: Limiting draw buffer count to 4 while FrameCapture enabled
```
## Trigger the capture

When you have reached the content in your application that you want to record, set the trigger
value to zero:
```
adb shell setprop debug.angle.capture.trigger 0
```
In logcat we'll see another entry corresponding to this:
```
ANGLE   : INFO: Capture triggered after frame 30440 for 10 frames
```
The app may pause briefly when the capture is completing. You can check its progress by looking at
the file system:
```
adb shell ls -la /sdcard/Android/data/$PACKAGE_NAME/angle_capture
```
Allow the app to run until the `*angledata.gz` file is non-zero and no longer growing. The app
should continue rendering after that:
```
$ adb shell ls -s -w 1 /sdcard/Android/data/$PACKAGE_NAME/angle_capture
30528 angry_birds_2_capture_context1.angledata.gz
    8 angry_birds_2_capture_context1.cpp
    4 angry_birds_2_capture_context1_files.txt
  768 angry_birds_2_capture_context1_frame001.cpp
  100 angry_birds_2_capture_context1_frame002.cpp
  100 angry_birds_2_capture_context1_frame003.cpp
  100 angry_birds_2_capture_context1_frame004.cpp
  100 angry_birds_2_capture_context1_frame005.cpp
  104 angry_birds_2_capture_context1_frame006.cpp
  100 angry_birds_2_capture_context1_frame007.cpp
  100 angry_birds_2_capture_context1_frame008.cpp
  100 angry_birds_2_capture_context1_frame009.cpp
  100 angry_birds_2_capture_context1_frame010.cpp
  120 angry_birds_2_capture_context1_frame011.cpp
    8 angry_birds_2_capture_context1.h
```
Note, you may see multiple contexts captured in the output. When this happens, look at the size of
the files. The larger files should be the context you care about it. You should move or delete the
other context files.

## Pull the trace files

Next, we want to pull those files over to the host and run some scripts.
```
cd $CHROMIUM_SRC/third_party/angle/src/tests/restricted_traces
mkdir -p $LABEL
adb pull /sdcard/Android/data/$PACKAGE_NAME/angle_capture/. $LABEL/
```

## Add the new trace to the JSON list

The list of traces is tracked in [restricted_traces.json](restricted_traces.json). Manually add your
new trace to this list. Use version "1" for the trace version.

You can also use a tool called `jq` to update the list. This ensures we get them in
alphabetical order with no duplicates. It can also be done by hand if you are unable to install it,
for some reason.
```
sudo apt-get install jq
```
Then run the following command:
```
export VERSION=1
jq ".traces = (.traces + [\"$LABEL $VERSION\"] | unique)" restricted_traces.json | sponge restricted_traces.json
```

## Run code auto-generation

We use two scripts to update the test harness so it will compile and run the new trace:
```
python ./gen_restricted_traces.py
cd ../../..
python ./scripts/run_code_generation.py
```
After this you should be able to `git diff` and see your new trace added to the harness files:
```
$ git diff --stat
 scripts/code_generation_hashes/restricted_traces.json     | 12 +++++++-----
 src/tests/restricted_traces/.gitignore                    |  2 ++
 src/tests/restricted_traces/restricted_traces.json        |  1 +
 src/tests/restricted_traces/restricted_traces_autogen.cpp | 19 +++++++++++++++++++
 src/tests/restricted_traces/restricted_traces_autogen.gni |  1 +
 src/tests/restricted_traces/restricted_traces_autogen.h   |  1 +
 6 files changed, 31 insertions(+), 5 deletions(-)
```
Note the absence of the traces themselves listed above.  They are automatically .gitignored since
they won't be checked in directly to the repo.

## Upload your trace to CIPD

Once you feel good about your trace, you can upload it to our collection of traces.  This can only
be done by Googlers with write access to the trace CIPD prefix. If you need write access contact
someone listed in the `OWNERS` file.

```
sync_restricted_traces_to_cipd.py
```

## Upload your CL

Ensure your current working directory is up-to-date, and upload:

```
git cl upload
```

You're now ready to run your new trace on CI!
