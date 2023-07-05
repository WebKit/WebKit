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
### Selecting which traces to build

Since the traces are numerous, you should limit compilation to a subset with the following GN arg:
```
angle_restricted_traces = ["among_us 5", "street_fighter_duel 1"]
```
If you choose not to pick any traces and build them all, you must follow different steps for Android. Skip ahead to [Building and running all traces for Android](#building-and-running-all-traces-for-android)

To build the trace tests:
```
autoninja -C out/<config> angle_trace_tests
```
## Running the trace tests
The trace tests can be run with default options like so:
```
out/<config>/angle_trace_tests
```
To select a specific trace to run, provide it with a filter:
```
out/<config>/angle_trace_tests --gtest_filter=TraceTest.<trace_name>
```
The specific options available with traces can be found in the PerfTests [`README`](../perf_tests/README.md#trace-tests)

Common options used are:
```
# Use ANGLE as the driver with the system's Vulkan driver as backend
--use-angle=vulkan

# Use the system's native GLES driver
--use-gl=native
```

### Building and running all traces for Android
Our trace library has gotten large enough that they no longer fit in a single APK.  To support building and running the entire library, we can compile the libraries by themselves, outside of the APK, and push them to locations accessible by the test harness.

To do so, remove `angle_restricted_traces` from your GN args, then compile with:
```
autoninja -C out/<config> angle_trace_perf_tests
```
and run with (including recommended options):
```
out/<config>/angle_trace_tests --filter='*among_us*' --verbose --fixed-test-time-with-warmup 10
```

If more than one device is connected, the target device serial should be provided as well:
```
ANDROID_SERIAL=<device_serial> out/<config>/angle_trace_tests ...
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
30528 angry_birds_2.angledata.gz
    8 angry_birds_2.cpp
    4 angry_birds_2.json
  768 angry_birds_2_001.cpp
  100 angry_birds_2_002.cpp
  100 angry_birds_2_003.cpp
  100 angry_birds_2_004.cpp
  100 angry_birds_2_005.cpp
  104 angry_birds_2_006.cpp
  100 angry_birds_2_007.cpp
  100 angry_birds_2_008.cpp
  100 angry_birds_2_009.cpp
  100 angry_birds_2_010.cpp
  120 angry_birds_2_011.cpp
    8 angry_birds_2.h
```

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

On Linux, you can also use a tool called `jq` to update the list. This ensures we get them in
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

The [`gen_restricted_traces`](gen_restricted_traces.py) script auto-generates entries
in our checkout dependencies to sync restricted trace data on checkout. To trigger
code generation run the following from the angle root folder:
```
python ./scripts/run_code_generation.py
```
After this you should be able to `git diff` and see changes in the following files:

 * `DEPS`
 * `scripts/code_generation_hashes/restricted_traces.json`
 * `src/tests/restricted_traces/restricted_traces.json` (this is the file you originally modified)

Note the absence of the traces themselves listed above. They are automatically
ignored by [`.gitignore`](.gitignore) since they won't be checked in directly
to the repo.

## Upload your trace to CIPD

Once you feel good about your trace, you can upload it to our collection of traces.  This can only
be done by Googlers with write access to the trace CIPD prefix. If you need write access contact
someone listed in the `OWNERS` file.

Please kindly go over the trace content with ANGLE code owners before running
below commands. You can share your trace through Google Drive for content
iterations. We cannot delete trace files once they are up on the CIPD.
Doing additional rounds of content check can help us save CIPD resources.

```
src/tests/restricted_traces/sync_restricted_traces_to_cipd.py
```

## Upload your CL

Ensure your current working directory is up-to-date, and upload:

```
git cl upload
```

You're now ready to run your new trace on CI!

# Upgrading existing traces

With tracer updates sometimes we want to re-run tracing to upgrade the trace file format or to
take advantage of new tracer improvements. The [`retrace_restricted_traces`](retrace_restricted_traces.py)
script allows us to re-run tracing using [SwiftShader](https://swiftshader.googlesource.com/SwiftShader)
on a desktop machine. As of writing we require re-tracing on a Windows machine because of size
limitations with a Linux app window.

## Prep work: Back up existing traces

This will save the original traces in a temporary folder if you need to revert to the prior trace format:

```
src/tests/restricted_traces/retrace_restricted_traces.py backup "*"
```

*Note: on Linux, remove the command `py` prefix to the Python scripts.*

This will save the traces to `./retrace-backups`. At any time you can revert the trace files by running:

```
src/tests/restricted_traces/retrace_restricted_traces.py restore "*"
```

## Part 1: Sanity Check with T-Rex

First we'll retrace a single app to verify the workflow is intact. Please
ensure you replace the specified variables with paths that work on your
configuration and checkout:

### Step 1/3: Capture T-Rex with Validation

```
export TRACE_GN_PATH=out/Debug
export TRACE_NAME=trex_200
src/tests/restricted_traces/retrace_restricted_traces.py upgrade $TRACE_GN_PATH retrace-wip -f $TRACE_NAME --validation --limit 3
```

The `--validation` flag will turn on additional validation checks in the
trace. The `--limit 3` flag forces a maximum of 3 frames of tracing so the
test will run more quickly. The trace will end up in the `retrace-wip`
folder.

### Step 2/3: Validate T-Rex

The command below will update your copy of the trace, rebuild, the run the
test suite with validation enabled:

```
src/tests/restricted_traces/retrace_restricted_traces.py validate $TRACE_GN_PATH retrace-wip $TRACE_NAME
```

If the trace failed validation, see the section below on diagnosing tracer
errors. Otherwise proceed with the steps below.

### Step 3/3: Restore the Canonical T-Rex Trace

```
src/tests/restricted_traces/retrace_restricted_traces.py restore $TRACE_NAME
```

## Part 2: Do a limited trace upgrade with validation enabled

### Step 1/3: Upgrade all traces with a limit of 3 frames

```
src/tests/restricted_traces/retrace_restricted_traces.py upgrade $TRACE_GN_PATH retrace-wip --validation --limit 3  --no-overwrite
```

If this process gets interrupted, re-run the upgrade command. The
`--no-overwrite` argument will ensure it will complete eventually.

If any traces failed to upgrade, see the section below on diagnosing tracer
errors. Otherwise proceed with the steps below.

### Step 2/3: Validate all upgraded traces

```
src/tests/restricted_traces/retrace_restricted_traces.py validate $TRACE_GN_PATH retrace-wip "*"
```

If any traces failed validation, see the section below on diagnosing tracer
errors. Otherwise proceed with the steps below.

### Step 3/3: Restore all traces

```
src/tests/restricted_traces/retrace_restricted_traces.py restore "*"
```

## Part 3: Do the full trace upgrade

```
rm -rf retrace-wip
src/tests/restricted_traces/retrace_restricted_traces.py upgrade $TRACE_GN_PATH retrace-wip --no-overwrite
```

If this process gets interrupted, re-run the upgrade command. The
`--no-overwrite` argument will ensure it will complete eventually.

If any traces failed to upgrade, see the section below on diagnosing tracer
errors. Otherwise proceed with the steps below.

## Part 4: Test the upgraded traces under an experimental prefix (slow)

Because there still may be trace errors undetected by validation, we first
upload the traces to a temporary CIPD path for testing. After a successful
run on the CQ, we will then upload them to the main ANGLE prefix.

To enable the experimental prefix, edit
[`restricted_traces.json`](restricted_traces.json) to use a version
number beginning with 'x'. For example:

```
  "traces": [
    "aliexpress x1",
    "among_us x1",
    "angry_birds_2_1500 x1",
    "arena_of_valor x1",
    "asphalt_8 x1",
    "avakin_life x1",
... and so on ...
```

Then run:

```
src/tests/restricted_traces/retrace_restricted_traces.py restore -o retrace-wip "*"
src/tests/restricted_traces/sync_restricted_traces_to_cipd.py
scripts/run_code_generation.py
```

The restore command will copy the new traces from the `retrace-wip` directory
into the trace folder before we call the sync script.

After these commands complete succesfully, create and upload a CL as normal.
Run CQ +1 Dry-Run. If you find a test regression, see the section below on
diagnosing tracer errors. Otherwise proceed with the steps below.

## Part 5: Upload the verified traces to CIPD under the stable prefix

Now that you've validated the traces on the CQ, update
[`restricted_traces.json`](restricted_traces.json) to remove the 'x' prefix
and incrementing the version of the traces (skipping versions if you prefer)
and then run:

```
src/tests/restricted_traces/sync_restricted_traces_to_cipd.py
scripts/run_code_generation.py
```

Then create and upload a CL as normal. Congratulations, you've finished the
trace upgrade!

## Finding a trace's minimum requirements

`retrace_restricted_traces.py` can be used to determine a trace's minimum
extensions and GLES version. Run the command:

```
src/tests/restricted_traces/retrace_restricted_traces.py get_min_reqs $TRACE_GN_PATH [--traces "*"]
```

The script will run each listed trace multiple times so it can find the minimum
required GLES version and each required extension. Finally it records that
information to the trace's json file.

By default it will run with SwiftShader. To make the script use your machine's
native vulkan drivers, use the `--no-swiftshader` argument before the script's
command:

```
src/tests/restricted_traces/retrace_restricted_traces.py --no-swiftshader get_min_reqs $TRACE_GN_PATH [--traces "*"]
```

# Diagnosing and fixing tracer errors

## Debugging a crash or GLES error

Ensure you're building ANGLE in Debug. Then look in the retrace script output
to find the exact command line and environment variables the script uses to
produce the failure. For example:

```
INFO:root:ANGLE_CAPTURE_LABEL=trex_200 ANGLE_CAPTURE_OUT_DIR=C:\src\angle\retrace-wip\trex_200 ANGLE_CAPTURE_FRAME_START=2 ANGLE_CAPTURE_FRAME_END=4 ANGLE_CAPTURE_VALIDATION=1 ANGLE_FEATURE_OVERRIDES_ENABLED=allocateNonZeroMemory:forceInitShaderVariables ANGLE_CAPTURE_TRIM_ENABLED=1 out\Debug\angle_trace_tests.exe --gtest_filter=TraceTest.trex_200 --use-angle=swiftshader --max-steps-performed 3 --retrace-mode
```

Once you can reproduce the issue you can use a debugger or other standard
debugging processes to find the root cause and a fix.

## Debugging a serialization difference

If you encouter a serialization mismatch in the retrace, you can find the
complete serialization output by looking in the retrace script output. ANGLE
saves the complete serialization file contents on any mismatch. You can
inspect and diff these files in a text editor to help diagnose what objects
are faulty.

If the mismatch is with a Buffer or Texture object content, you can manually
edit the `frame_capture_utils.cpp` file to force some or all of the objects
to serialize their entire contents. This can help show what kind of pixel or
data differences might be causing the issue. For example, change this line:

```
json->addBlob("data", dataPtr->data(), dataPtr->size());
```

to

```
json->addBlobWithMax("data", dataPtr->data(), dataPtr->size(), 1000000);
```

Note: in the future, we might make this option exposed via an envioronment
variable, or even allow serialization of entire data blocks in text-encoded
form that could be decoded to separate files.

If you still can't determine what code might be causing the state difference,
we can insert finer-grained serialization checkpoints to "bisect" where the
coding mismatch is happening. It is not possible to force checkpoints after
every GLES call, because serialization and validation is so prohibitively
expensive. ANGLE instead has feature in the tracer that allows us to
precisely control where the tracer inserts and validates the checkpoints, by
using a boolean expression language.

The retrace script command `--validation-expr` allows us to specify a C-like
expression that determines when to add serialization checkpoints. For
example, we can specify this validation expression:

```
((frame == 2) && (call < 1189) && (call > 1100) && ((call % 5) == 0))
```

Using this expression will insert a serialization checkpoint in the second
frame, on every 5th captured call, and when the captured call count is
between 1101 and 1188. Here the `call` keyword denotes the call counter,
which resets to 1 every frame, and increments by 1 with every captured GLES
API call. The `frame` keyword denotes the frame counter, which starts at 1
and increments by 1 every captured frame. The expression syntax supports all
common C boolean operators.

By finding a starting and ending frame range, and narrowing this range through
experimentation, you can pinpoint the exact call that triggers the
serialization mismatch, and then diagnose and fix the root cause. In some
cases you can use RenderDoc or other frame debugging tools to inspect
resource states before/after the bad call once you have found it.

See also: [`http://crrev.com/c/3136094`](http://crrev.com/c/3136094)

## Debugging a pixel test failure without a serialization mismatch

Sometimes you manage to complete validation and upload, just to find a golden
image pixel difference that manifests in some trace configurations. These
problems can be harder to root cause. For instance, some configurations may
render undefined pixels that are in practice well-defined on most GLES
implementations.

The pixel differences can also be a product of mismatched state even if the
trace validation says all states are matched. Because ANGLE's GLES state
serialization is incomplete, it can help to check the state serialization
logic and add missing features as necessary.