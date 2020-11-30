# ANGLE Restricted Traces

The files in this directory are traces of real applications. We host them
internally because they may contain third party IP which we don't want
to share publicly.

## Setup

In order to compile and run with these, you must be granted access by Google,
then authenticate with the cloud with your @google account:
```
download_from_google_storage --config
<enter 0 for the project ID>
```
Add the following to ANGLE's .gclient file:
```
    "custom_vars": {
      "checkout_angle_internal":"True"
    },
```
Then use gclient to pull down binary files from a cloud storage bucket.
```
gclient runhooks
```
To build the tests, add the following GN argument:
```
build_angle_trace_perf_tests = true
```
Build the angle_perftests:
```
autoninja -C out/Release angle_perftests
```
Run them like so:
```
out/Release/angle_perftests --gtest_filter=TracePerfTest*
```

## Updating traces

The GFXBench traces were captured on Windows after building gfxbench from source. We run capture commands in git bash.

### Windows prep

Launch git bash via the 'git bash' command or via the start menu. Then run:

```
export PLATFORM=vs2019-x64
export TFW_PACKAGE_DIR=../../../build/vs2019-x64/testfw/tfw-dev
export ANGLE_DEFAULT_PLATFORM=vulkan
```

### Linux prep

```
export PLATFORM=linux
export LD_LIBRARY_PATH=.
export TFW_PACKAGE_DIR=../../../build/linux/testfw_Release/tfw-dev
export ANGLE_DEFAULT_PLATFORM=vulkan
```

### Capturing from gfxbench

First start from the gfxbench source directory.

```
cd out/install/$PLATFORM/lib
python ../../../../scripts/refresh_angle_libs.py

# TRex
mkdir -p ../../../../../angle/src/tests/perf_tests/restricted_traces/trex_200
ANGLE_CAPTURE_OUT_DIR=../../../../../angle/src/tests/perf_tests/restricted_traces/trex_200 ANGLE_CAPTURE_FRAME_START=200 ANGLE_CAPTURE_FRAME_END=210 ANGLE_CAPTURE_LABEL=trex_200 ../bin/testfw_app -b $TFW_PACKAGE_DIR --gfx egl -w 1920 -h 1080 -t gl_trex --ei -frame_step_time=40

# Manhattan
mkdir -p ../../../../../angle/src/tests/perf_tests/restricted_traces/manhattan_10
ANGLE_CAPTURE_OUT_DIR=../../../../../angle/src/tests/perf_tests/restricted_traces/manhattan_10 ANGLE_CAPTURE_FRAME_START=10 ANGLE_CAPTURE_FRAME_END=20 ANGLE_CAPTURE_LABEL=manhattan_10 ../bin/testfw_app -b $TFW_PACKAGE_DIR --gfx egl -w 1920 -h 1080 -t gl_manhattan --ei -frame_step_time=40

# Egypt
mkdir -p ../../../../../angle/src/tests/perf_tests/restricted_traces/egypt_1500
ANGLE_CAPTURE_OUT_DIR=../../../../../angle/src/tests/perf_tests/restricted_traces/egypt_1500 ANGLE_CAPTURE_FRAME_START=1500 ANGLE_CAPTURE_FRAME_END=1510 ANGLE_CAPTURE_LABEL=egypt_1500 ../bin/testfw_app -b $TFW_PACKAGE_DIR --gfx egl -w 1920 -h 1080 -t gl_egypt --ei -frame_step_time=40
```

## Upload to the cloud

Starting from you ANGLE root directory:

```
cd src/tests/perf_tests/restricted_traces
upload_to_google_storage.py --bucket chrome-angle-capture-binaries --archive trex_200
upload_to_google_storage.py --bucket chrome-angle-capture-binaries --archive manhattan_10
upload_to_google_storage.py --bucket chrome-angle-capture-binaries --archive egypt_1500
```

After uploading, add the sha1 files created by the upload and submit them with your changes.

## Adding new tests

After you capture and upload a set of frames from an application using the steps above you'll need to:

 * Add the new traces to [`restricted_traces/restricted_traces.json`](restricted_traces.json).
 * Re-run code generation with [`scripts/run_code_generation.py`][run_code_generation].
 * Update your CL to include the new generated code. Your trace tests should now be turned on.

[run_code_generation]: ../../../../scripts/run_code_generation.py
