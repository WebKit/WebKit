# ANGLE OpenGL Frame Capture and Replay

ANGLE currently supports a limited OpenGL capture and replay framework.

Limitations:

 * GLES capture has many unimplemented functions.
 * EGL capture and replay is not yet supported.
 * Mid-execution capture is supported with the Vulkan back-end.
 * Mid-execution capture has many unimplemented features.
 * Capture and replay is currently only tested on desktop platforms.
 * Binary replay is unimplemented. CPP replay is supported.

## Capturing and replaying an application

To build ANGLE with capture and replay enabled update your GN args:

```
angle_with_capture_by_default = true
```

Once built ANGLE will capture the OpenGL ES calls to CPP replay files. By default the replay will be
stored in the current working directory. The capture files will be named according to the pattern
`angle_capture_context{id}_frame{n}.cpp`. Each GL Context currently has its own replay sources.
ANGLE will write out data binary blobs for large Texture or Buffer contents to
`angle_capture_context{id}_frame{n}.angledata`. Replay programs must be able to load data from the
corresponding `angledata` files.

## Controlling Frame Capture

Some simple environment variables control frame capture:

 * `ANGLE_CAPTURE_ENABLED`:
   * Set to `0` to disable capture entirely. Default is `1`.
 * `ANGLE_CAPTURE_COMPRESSION`:
   * Set to `0` to disable capture compression. Default is `1`.
 * `ANGLE_CAPTURE_OUT_DIR=<path>`:
   * Can specify an alternate replay output directory.
   * Example: `ANGLE_CAPTURE_OUT_DIR=samples/capture_replay`. Default is the CWD.
 * `ANGLE_CAPTURE_FRAME_START=<n>`:
   * Uses mid-execution capture to write "Setup" functions that starts a Context at frame `n`.
   * Example: `ANGLE_CAPTURE_FRAME_START=2`. Default is `0`.
 * `ANGLE_CAPTURE_FRAME_END=<n>`:
   * By default ANGLE will capture the first ten frames. This variable can override the default.
   * Example: `ANGLE_CAPTURE_FRAME_END=4`. Default is `10`.
 * `ANGLE_CAPTURE_LABEL=<label>`:
   * When specified, files and functions will be labeled uniquely.
   * Example: `ANGLE_CAPTURE_LABEL=foo`
     * Results in filenames like this:
       ```
       foo_capture_context1.cpp
       foo_capture_context1.h
       foo_capture_context1_files.txt
       foo_capture_context1_frame000.angledata
       foo_capture_context1_frame000.cpp
       foo_capture_context1_frame001.angledata
       foo_capture_context1_frame001.cpp
       ...
       ```
     * Functions wrapped in namespaces like this:
       ```
       namespace foo
       {
           void ReplayContext1Frame0();
           void ReplayContext1Frame1();
       }
       ```
     * For use like this:
       ```
       foo::SetupContext1Replay();
       for (...)
       {
           foo::ReplayContext1Frame(i);
       }
       ```

A good way to test out the capture is to use environment variables in conjunction with the sample
template. For example:

```
$ ANGLE_CAPTURE_FRAME_END=4 ANGLE_CAPTURE_OUT_DIR=samples/capture_replay out/Debug/simple_texture_2d
```

## Running a CPP replay

To run a CPP replay you can use a template located in
[samples/capture_replay](../samples/capture_replay). First run your capture and ensure all capture
files are written to `samples/capture_replay`. You can conveniently use `ANGLE_CAPTURE_OUT_DIR`.
Then enable the `capture_replay_sample` via `gn args`:

```
angle_build_capture_replay_sample = true
```

See [samples/BUILD.gn](../samples/BUILD.gn) for details. Then build and run your replay sample:

```
$ autoninja -C out/Debug capture_replay_sample
$ ANGLE_CAPTURE_ENABLED=0 out/Debug/capture_replay_sample
```

Note that we specify `ANGLE_CAPTURE_ENABLED=0` to prevent re-capturing when running the replay.

## Capturing an Android application

In order to capture on Android, the following additional steps must be taken. These steps
presume you've built and installed the ANGLE APK with capture enabled, and selected ANGLE
as the GLES driver for your application.

1. Create the output directory

    Determine your package name:
    ```
    export PACKAGE_NAME com.android.gl2jni
    ```
    Then create an output directory that it can write to:
    ```
    $ adb shell mkdir -p /sdcard/Android/data/$PACKAGE_NAME/angle_capture
    ```

2. Set properties to use for environment variable

    On Android, it is difficult to set an environment variable before starting native code.
    To work around this, ANGLE will read debug system properties before starting the capture
    and use them to prime environment variables used by the capture code.

    Note: Mid-execution capture doesn't work for Android just yet, so frame_start must be
    zero, which is the default. This it is sufficient to only set the end frame.
    ```
    $ adb shell setprop debug.angle.capture.frame_end 200
    ```

    There are other properties that can be set that match 1:1 with the env vars, but
    they are not required for capture:
    ```
    # Optional
    $ adb shell setprop debug.angle.capture.enabled 0
    $ adb shell setprop debug.angle.capture.out_dir foo
    $ adb shell setprop debug.angle.capture.frame_start 0
    $ adb shell setprop debug.angle.capture.label bar
    ```

3.  Run the application, then pull the files to the capture_replay directory
    ```
    $ cd samples/capture_replay
    $ adb pull /sdcard/Android/data/$PACKAGE_NAME/angle_capture replay_files
    $ cp replay_files/* .
    ```

4. Update your GN args to specifiy which context will be replayed.

    By default Context ID 1 will be replayed. On Android, Context ID 2 is more typical, some apps
    we've run go as high as ID 6.
    Note: this solution is temporary until EGL capture is in place.
    ```
    angle_capture_replay_sample_context_id = 2
    ```

5. Replay the capture on desktop

    Until we have samples building for Android, the replay sample must be run on desktop.
    We will also be plumbing replay files into perf and correctness tests which will run on Android.
    ```
    $ autoninja -C out/Release capture_replay_sample
    $ out/Release/capture_replay_sample
    ```