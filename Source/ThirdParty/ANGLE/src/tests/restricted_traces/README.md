# ANGLE Restricted Traces

The files in this directory are traces of real applications. We host them
internally because they may contain third party IP which we don't want
to share publicly.

## Accessing the traces

In order to compile and run with these, you must be granted access by Google,
then authenticate with [CIPD](https://chromium.googlesource.com/infra/luci/luci-go/+/main/cipd/README.md). Googlers, use your @google account.
```
cipd auth-login
```
Add the following to ANGLE's .gclient file:
```
    "custom_vars": {
      "checkout_angle_restricted_traces": True
    },
```

Note: alternatively, you can checkout only a few specific traces using the following format (`angle_restricted_traces` in gn args below should be a matching list or a subset):
```
    "custom_vars": {
      "checkout_angle_restricted_trace_{trace_name_1}": True,
      "checkout_angle_restricted_trace_{trace_name_2}": True,
      ...
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

If capturing a new trace using OpenCL, also add the following to your Debug setup:
```
angle_enable_cl = true
```

<details>
  <summary>Example of full OpenCL GN arg Debug setup for Capture</summary>
  <br>

    # Target information
    is_clang = true
    target_cpu = "arm64"
    target_os = "android"

    # Enable CL + backends
    angle_enable_cl = true
    angle_enable_vulkan = true

    # Debug mode flags
    is_debug = true
    symbol_level = 2
    strip_debug_info = false
    android_full_debug = true
    ignore_elf32_limitations = true

    # Other flag settings
    is_official_build = false
    is_component_build = false
    angle_extract_native_libs = true
</details>

## Capturing long traces

The tracer will capture traces larger than the available physical memory on the
target device. However, for very long traces there may be noticeable hitching
during disk-writes once available memory is exhausted. If this becomes problematic a
release build can be used, or to minimize hitching completely, disable binary data
file compression. To do this ensure that the following property is set for capture:
```
adb shell setprop debug.angle.capture.compression 0
```
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

Note: If running an executable, not an application/APK, these settings don't apply.

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

For OpenCL capture, a trigger most likely won't be wanted. So, set the frame_start and frame_end values accordingly. Each ```clEnqueueNDRangeKernel``` is considered the end of a frame.
```
adb shell setprop debug.angle.capture.frame_start 1
adb shell setprop debug.angle.capture.frame_end 100.
```

But if you do want to capture OpenCL and the end frame isn't clear, use the end_capture feature. See [End the capture early](./README.md#end-the-capture-early) for more information.

## Create output location

We need to write out the trace file in a location accessible by the app. We use the app's data
storage on sdcard, but create a subfolder to isolate ANGLE's files:
```
adb shell mkdir -p /sdcard/Android/data/$PACKAGE_NAME/angle_capture
```

If the [capture](#Trigger the capture) failed due
to these errors:

```
Output directory
'/sdcard/Android/data/$PACKAGE_NAME/angle_capture/' does not exist.
Create it over adb using mkdir.
```

```
Could not open binary data file /sdcard/Android/data/$PACKAGE_NAME/angle_capture/$LABEL.angledata.gz
```

It means the app does not have access to /sdcard/. We will need to use the app's
own data storage, which requires enabling adb root first:

```
adb root
adb shell setprop debug.angle.capture.out_dir /data/data/$PACKAGE_NAME/angle_capture
adb shell mkdir -p /data/data/$PACKAGE_NAME/angle_capture
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

Note: If you have set the start and end frame, this step does not apply.

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
Allow the app to run until the logcat entry indicating the end of the API
capture. The app should continue rendering after that:
```
ANGLE   : INFO: Finished recording graphics API capture
```

## Optionally trigger additional captures

It is possible to capture an arbitrary number of traces.

After each trace completes, set the trigger value to the desired number of frames to capture
for the next trace, optionally create and specify a different out_dir for the new trace data,
and then start the new trace by again resetting the trigger value to zero.

Example workflow for multiple captures:

```
adb shell mkdir -p /data/data/$PACKAGE_NAME/angle_capture_1
adb shell mkdir -p /data/data/$PACKAGE_NAME/angle_capture_2
adb shell mkdir -p /data/data/$PACKAGE_NAME/angle_capture_3

# Set initial output dir and frame count
adb shell setprop debug.angle.capture.out_dir /data/data/$PACKAGE_NAME/angle_capture_1
adb shell setprop debug.angle.capture.trigger 100

# Trigger capture
adb shell setprop debug.angle.capture.trigger 0

# Set the next output dir and frame count
adb shell setprop debug.angle.capture.out_dir /data/data/$PACKAGE_NAME/angle_capture_2
adb shell setprop debug.angle.capture.trigger 30

# Trigger capture
adb shell setprop debug.angle.capture.trigger 0

# Set the next output dir and frame count
adb shell setprop debug.angle.capture.out_dir /data/data/$PACKAGE_NAME/angle_capture_3
adb shell setprop debug.angle.capture.trigger 60

# Trigger capture
adb shell setprop debug.angle.capture.trigger 0

# Pull the traces
adb pull /data/data/$PACKAGE_NAME/angle_capture_1
adb pull /data/data/$PACKAGE_NAME/angle_capture_2
adb pull /data/data/$PACKAGE_NAME/angle_capture_3
```

Note that multiple captures are incompatible with applications using persistent coherent memory.
If more than one capture is attempted in this situation the tracer will exit immediately.
The initial capture will remain valid.

## End the capture early

If the application doesn't have a clear known ending frame, use ```debug.angle.capture.end_capture```.

Set frame_start. frame_end is irrelevant for ending the capture early.
```
adb shell setprop debug.angle.capture.frame_start 1
```
Set end_capture to any number greater than 0. The value doesn't matter, as long as it's greater than 0.
```
adb shell setprop debug.angle.capture.end_capture 1
```
Run the application. At the moment you want the capture to be done, set end_capture to 0
```
adb shell setprop debug.angle.capture.end_capture 0
```
This will capture everything up to the time you triggered the end of the capture.

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

# Comparing screenshots against the native driver

To compare trace screenshots from ANGLE vs the native driver, you can
use the `compare_trace_screenshots.py` script.

The following steps will work on Android, but are transferrable to
any operating system.

First, make a spot for the results:
```
adb shell rm -r /sdcard/angle/screenshots
adb shell mkdir -p /sdcard/angle/screenshots
```

Then run the traces using ANGLE:
```
out/AndroidPerformance/angle_trace_tests --verbose --run-to-key-frame --screenshot-dir /sdcard/angle/screenshots --shard-timeout 1000000
```

And again with the native driver:
```
out/AndroidPerformance/angle_trace_tests --verbose --run-to-key-frame --screenshot-dir /sdcard/angle/screenshots --shard-timeout 1000000 --use-gl=native
```

It may take a few tries as some drivers will crash.  In that case, run the ones at the end that were stragglers, i.e.:
```
out/AndroidPerformance/angle_trace_tests --verbose --run-to-key-frame --screenshot-dir /sdcard/angle/screenshots --shard-timeout 1000000 --use-gl=native --gtest_filter="*words*:*world*:*worms*:*zenonia*:*zillow*:*zombie*"
```

Pull the screenshots:
```
adb pull /sdcard/angle/screenshots
cd screenshots
```

And run the compare script:
```
python3 ../src/tests/restricted_traces/compare_trace_screenshots.py versus_native --screenshot-dir . --trace-list-path ../src/tests/restricted_traces/
```

The script will print out results comparing ANGLE vs. native screenshots at different fuzz factors.
It may also print out NA for missing screenshots:
```
...
arknights angle_vulkan_arknights.png MISSING_EXT.png NA NA NA NA NA NA
asphalt_8 angle_vulkan_asphalt_8.png angle_native_asphalt_8.png 641849 222157 116426 1701 82 22
asphalt_9 angle_vulkan_asphalt_9.png angle_native_asphalt_9.png 17919 420 305 293 232 3
...
```

Script will also save difference PNG files for each fuzz factor into the `--screenshot-dir`. These
files will be saved even if there is no difference. To discard such files you may add
`--discard_zero_diff_png` (or `-d`) argument **before** the `versus_native` command.

# Comparing screenshots in two directories at different fuzz factors

In some cases it may be useful to compare screenshots in two directories. For example: to compare
different runs on the same device, different loops of the same replay run, runs on different
devices, and so on.

After you have prepared screenshots for comparison, run the `fuzz_ab` command:

```
python3 compare_trace_screenshots.py -d fuzz_ab --a_dir /my/trace/a --b_dir /my/trace/b --out /my/trace/diff
```

It will compare screenshots in these directories at different fuzz factors (similar to the
`versus_native` command). In this example `-d` (`--discard_zero_diff_png`) argument was added to
only keep PNG files with the difference.

Command also has `--relaxed_file_list_match` (`-r`) argument, allowing to compare directories with
at least single match, which may be handy in some situations.

# Upgrading existing traces

With tracer updates sometimes we want to re-run tracing to upgrade the trace file format or to
take advantage of new tracer improvements. The [`retrace_restricted_traces`](retrace_restricted_traces.py)
script allows us to re-run tracing using [SwiftShader](https://swiftshader.googlesource.com/SwiftShader)
on a desktop machine. As of writing we require re-tracing on a Windows machine because of size
limitations with a Linux app window.

## Part 1: Retrace it

Upgrade your trace into a new directory called `retrace-wip`

In this instance, we'll upgrade `octopath_traveler`
```
export TRACE_GN_PATH=out/Debug
export TRACE_NAME=octopath_traveler
src/tests/restricted_traces/retrace_restricted_traces.py upgrade $TRACE_GN_PATH retrace-wip -f $TRACE_NAME
```

### Interleaved client attribute analysis
Apps can use client attribute arrays without binding to the array buffer (e.g., via glVertexAttribPointer())
to draw.
The capture tool has been updated to detect interleaved client vertex attributes, i.e., vertex attributes that
use the same client array data at different offsets for draw. In that case, the client array data is captured
along with the offsets for each attribute. However, this will not be reflected on the traces captured before
this feature was introduced. In those traces, each client attribute data is recorded as a separate attribute
array (`gClientArrays[]`) and treated as an attribute separate from the rest, which can result in different
behavior between the trace and the real app (such as instruction count).

For example, take a draw using three interleaved client array pointers, each using a 4-byte float value.
* In the real app, this would look like three glVertexAttribPointer() calls with the pointer
set to `ptr`, `ptr + 4` and `ptr + 8`.
* However, before this feature was introduced in the capture tool, these attributes would be recorded as
`gClientArrays[0]`, `gClientArrays[1]`, and `gClientArrays[2]`.

The script [`check_attribute_interleaving.py`](check_attribute_interleaving.py) has been added to close the gap
further between the trace and the real app, which the retracing script will use during an upgrade to analyze the
client array attribute pointers in the trace (`gClientArrays[]`) and their corresponding data.
Its goal is to detect and restore the interleaved property of such attributes.

If the attributes were found to have matching data with an offset, it will print a notification indicating the
existence of such cases in the existing trace. However, it will **not** try to merge those attributes yet.
**To apply the attribute merges to the trace code during the upgrade, the flag `-m` or `--merge-attributes`
should be used for the retracing script:**
```
src/tests/restricted_traces/retrace_restricted_traces.py upgrade $TRACE_GN_PATH retrace-wip -m -f $TRACE_NAME
```

After this, the attributes determined to be part of the same client array are updated to use the same unified
data with their respective offsets.

Note that the script [`check_attribute_interleaving.py`](check_attribute_interleaving.py) can also be used as a
standalone tool to analyze trace code for potential interleaved client attributes.

## Part 2: Verify it

Before we check in an upgraded trace, we want to put it through enough paces to
ensure behaves the same or better.

### Screenshots

For screenshots, we want to verify all frames render correctly before and after Reset.

So make two spots to gather the screenshots, and one to gather results:
```
mkdir retrace-wip/${TRACE_NAME}_before
mkdir retrace-wip/${TRACE_NAME}_after
mkdir retrace-wip/${TRACE_NAME}_compare
```

We need two loops to verify Reset, so you'll need to inspect how many frames
are in the trace. In this case, `octopath_traveler` has 500 frames, so we need
1000 screenshots. We use -1 as the screenshot frame so we get all images:
```
export FRAME_COUNT=1000
out/Debug/angle_trace_tests --gtest_filter=TraceTest.${TRACE_NAME} --use-angle=swiftshader --max-steps-performed ${FRAME_COUNT} --screenshot-dir retrace-wip/${TRACE_NAME}_before --screenshot-frame -1
```

Then move the new trace in and run it again:
```
mv src/tests/restricted_traces/${TRACE_NAME} retrace-wip/${TRACE_NAME}_orig
cp -r retrace-wip/${TRACE_NAME} src/tests/restricted_traces
autoninja -C out/Debug angle_trace_tests
out/Debug/angle_trace_tests --gtest_filter=TraceTest.${TRACE_NAME} --use-angle=swiftshader --max-steps-performed ${FRAME_COUNT} --screenshot-dir retrace-wip/${TRACE_NAME}_after --screenshot-frame -1
```

After that, we have a script that will compare the before and after screenshots,
saving the results:
```
vpython3 src/tests/restricted_traces/compare_trace_screenshots.py versus_upgrade --before retrace-wip/${TRACE_NAME}_before --after retrace-wip/${TRACE_NAME}_after --outdir retrace-wip/${TRACE_NAME}_compare
```

If you have any diffs, they will pop out like this, and you need to investigate:
```
angle_vulkan_swiftshader_octopath_traveler_frame1.png 0
angle_vulkan_swiftshader_octopath_traveler_frame10.png 0
angle_vulkan_swiftshader_octopath_traveler_frame100.png 1.12185e+06
Pixel diff detected!
```

### Performance

We need to ensure we're getting the same frame times and memory usage.

The easiest way to do that is on Android, which can show us GPU and CPU memory.

First, restore the original trace, then build and install the most optimized build:
```
rm -r src/tests/restricted_traces/${TRACE_NAME}
cp -r retrace-wip/${TRACE_NAME}_orig src/tests/restricted_traces/${TRACE_NAME}
autoninja -C out/AndroidPerformance angle_trace_tests
```

Then run the `restricted_trace_perf.py` script to gather frame times and memory:
```
pushd src/tests/restricted_traces
vpython3 restricted_trace_perf.py --fixedtime 10 --sleep 10 --memory --cpu-inst user_and_kernel --output-tag ${TRACE_NAME}.before --loop-count 5 --build-dir ../../../out/AndroidPerformance --renderer vulkan --filter ${TRACE_NAME}
popd
```

You should get output like this:
```
trace                                    wall_time       gpu_time        frame_wall_time cpu_time        gpu_power  cpu_power  infra_power gpu_mem_sustained    gpu_mem_peak    proc_mem_median      proc_mem_peak   process_cpuinst      gfxlib_cpuinst       angle_cpuinst        vulkan_cpuinst       gles_cpuinst         libc_cpuinst

Starting run 1 with vulkan at 2023-08-17 16:26:29

vulkan_octopath_traveler                 2.8125          0               2.0213284872    2.6833208333    0.000      0.000      0.000      157458432            157458432       581324000            581536000       9814450.073333334    1907805.1666666667   30495.19861111111    0.0                  1877309.9680555556   9814093.243888889

Starting run 2 with vulkan at 2023-08-17 16:26:54

vulkan_octopath_traveler                 2.7942          0               2.0026349672    2.6582661111    0.000      0.000      0.000      157167616            157167616       580976000            581120000       9940974.378611112    1930372.9425         29421.109166666665   0.0                  1900951.8333333333   9940970.091666667

Starting run 3 with vulkan at 2023-08-17 16:27:18

vulkan_octopath_traveler                 2.8087          0               2.0104668258    2.6753233333    0.000      0.000      0.000      155391744            155398144       578784000            578844000       9849789.568611111    1897764.1875         27965.308333333334   0.0                  1869798.8791666667   9849162.080555556

Starting run 4 with vulkan at 2023-08-17 16:27:42

vulkan_octopath_traveler                 2.8126          0               2.0338224075    2.6839755556    0.000      0.000      0.000      148413982            157356032       580764000            580808000       9858647.521944445    1891151.5491666666   27062.17777777778    0.0                  1864089.371388889    9857876.606944444

Starting run 5 with vulkan at 2023-08-17 16:28:05

vulkan_octopath_traveler                 2.8193          0               2.0375366431    2.6894483333    0.000      0.000      0.000      148760576            148770816       572796000            572820000       9813130.877222221    1892703.2255555557   25800.17             0.0                  1866903.0555555555   9812411.621388888
```

Bring in the upgraded trace, build and install the trace again:
```
rm -rf src/tests/restricted_traces/${TRACE_NAME}
cp -r retrace-wip/${TRACE_NAME} src/tests/restricted_traces/${TRACE_NAME}
autoninja -C out/AndroidPerformance angle_trace_tests
```

And collect performance data:
```
vpython3 restricted_trace_perf.py --fixedtime 10 --sleep 10 --memory --cpu-inst user_and_kernel --output-tag ${TRACE_NAME}.after --loop-count 5 --build-dir ../../../out/AndroidPerformance --renderer vulkan --filter ${TRACE_NAME}
```

Verify using a spreadsheet that the values are relatively the same.
If you notice a marked difference, spend some time understanding it.
For instance, you may see memory decrease due to fixed in the upgrade.

## Part 3: Test the upgraded traces under an experimental prefix

To test the trace on all platforms, we first upload them to a temporary CIPD
path for testing. After a successful run on the CQ, we will then upload them
to the main ANGLE prefix.

To enable the experimental prefix, edit
[`restricted_traces.json`](restricted_traces.json) to use a version
number beginning with 'x'. For example:

```
  "traces": [
    ...
    "octopath_traveler x1",
    ...
```

Then run:

```
vpython3 src/tests/restricted_traces/sync_restricted_traces_to_cipd.py --filter ${TRACE_NAME}
vpython3 scripts/run_code_generation.py
```

After these commands complete succesfully, create and upload a CL as normal.
```
git cl upload
```

Before running tests, you need to grant the bots access to your experimental
CIPD files (substituting your account name):
```
cipd acl-edit experimental/google.com/$USERNAME -reader user:angle-try-builder@chops-service-accounts.iam.gserviceaccount.com
cipd acl-edit experimental/google.com/$USERNAME -reader user:chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com
```

You can verify it worked by running this command and seeing the bot added to readers:
```
cipd acl-list experimental/google.com/$USERNAME/angle/traces
...
Readers:
  via "experimental/google.com/$USERNAME":
    user:angle-try-builder@chops-service-accounts.iam.gserviceaccount.com
    user:chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com
```

Run CQ +1 Dry-Run. If you find a test regression, see the section below on
diagnosing tracer errors. Otherwise proceed with the steps below.

## Part 4: Upload the verified traces to CIPD under the stable prefix

Now that you've validated the traces on the CQ, update
[`restricted_traces.json`](restricted_traces.json) to remove the 'x' prefix
and incrementing the version of the traces (skipping versions if you prefer)
and then run:

```
vpython3 src/tests/restricted_traces/sync_restricted_traces_to_cipd.py --filter ${TRACE_NAME}
vpython3 scripts/run_code_generation.py
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

If retracing an existing trace, any associated `addExtensionPrerequisite()` calls must be removed from `TracePerfTest.cpp` and
the tracename.json file must be made writable.

Traces are run with all extensions enabled by default. It may be useful to test with only a subset of extensions.
This can be done by adding the `--request-extensions` argument to `angle_trace_tests`. Multiple extensions must be contained by quotation
marks and only a single space can be used as a separator. To run with no extensions enabled, specify a null list -- `""`:

```
./out/Debug/angle_trace_tests --gtest_filter=*tracename --request-extensions "EXT_color_buffer_float GL_EXT_texture_filter_anisotropic"
```
  or
```
./out/Debug/angle_trace_tests --gtest_filter=*tracename --request-extensions ""
```

## Extended testing and full trace upgrades

If you want to really drill down on trace differences, you might want to use the
built in validation support, which serailizes the internal state of ANGLE.

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

## Part 1: Upgrading Sanity Check with T-Rex

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
errors.

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
errors.

Otherwise, use the steps above to [verify and upgrade your traces](#part-3-test-the-upgraded-traces-under-an-experimental-prefix).


# Diagnosing and fixing tracer errors

## Debugging a crash or GLES error

Ensure you're building ANGLE in Debug. Then look in the retrace script output
to find the exact command line and environment variables the script uses to
produce the failure. For example:

```
INFO:root:ANGLE_CAPTURE_LABEL=trex_200 ANGLE_CAPTURE_OUT_DIR=C:\src\angle\retrace-wip\trex_200 ANGLE_CAPTURE_FRAME_START=2 ANGLE_CAPTURE_FRAME_END=4 ANGLE_CAPTURE_VALIDATION=1 ANGLE_FEATURE_OVERRIDES_ENABLED=allocateNonZeroMemory:forceInitShaderVariables out\Debug\angle_trace_tests.exe --gtest_filter=TraceTest.trex_200 --use-angle=swiftshader --max-steps-performed 3 --retrace-mode
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
