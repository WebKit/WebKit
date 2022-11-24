# ANGLE Performance Tests

`angle_perftests` is a standalone microbenchmark testing suite that contains
tests for the OpenGL API. `angle_trace_tests` is a suite to run captures traces for correctness and
performance. Because the traces contain confidential data, they are not publicly available. 
For more details on ANGLE's tracer please see the [docs](../restricted_traces/README.md).

The tests currently run on the Chromium ANGLE infrastructure and report
results to the [Chromium perf dashboard](https://chromeperf.appspot.com/report).
 Please refer to the[public dashboard docs][DashboardDocs] for help

[DashboardDocs]: https://chromium.googlesource.com/catapult/+/HEAD/dashboard/README.md

## Running the Tests

You can follow the usual instructions to [check out and build ANGLE](../../../doc/DevSetup.md).
 Build the `angle_perftests` or `angle_trace_tests` targets. Note that all
test scores are higher-is-better. You should also ensure `is_debug=false` in
your build. Running with `angle_assert_always_on` or debug validation enabled
is not recommended.

Variance can be a problem when benchmarking. We have a test harness to run a
tests repeatedly to find a lower variance measurement. See [`src/tests/run_perf_tests.py`][RunPerfTests].

To use the script first build `angle_perftests` or `angle_trace_tests`, set
your working directory your build directory, and invoke the
`run_perf_tests.py` script. Use `--test-suite` to specify your test suite,
and `--filter` to specify a test filter.

[RunPerfTests]: https://chromium.googlesource.com/angle/angle/+/main/scripts/perf_test_runner.py

### Choosing the Test to Run

You can choose individual tests to run with `--gtest_filter=*TestName*`. To
select a particular ANGLE back-end, add the name of the back-end to the test
filter. For example: `DrawCallPerfBenchmark.Run/gl` or
`DrawCallPerfBenchmark.Run/d3d11`. Many tests have sub-tests that run
slightly different code paths. You might need to experiment to find the right
sub-test and its name.

### Null/No-op Configurations

ANGLE implements a no-op driver for OpenGL, D3D11 and Vulkan. To run on these
configurations use the `gl_null`, `d3d11_null` or `vulkan_null` test
configurations. These null drivers will not do any GPU work. They will skip
the driver entirely. These null configs are useful for diagnosing performance
overhead in ANGLE code.

### Command-line Arguments

Several command-line arguments control how the tests run:

* `--one-frame-only`: Runs tests once and quickly exits. Used as a quick smoke test.
* `--enable-trace`: Write a JSON event log that can be loaded in Chrome.
* `--trace-file file`: Name of the JSON event log for `--enable-trace`.
* `--calibration`: Prints the number of steps a test runs in a fixed time. Used by `perf_test_runner.py`.
* `--steps-per-trial x`: Fixed number of steps to run for each test trial.
* `--max-steps-performed x`: Upper maximum on total number of steps for the entire test run.
* `--render-test-output-dir=dir`: Directory to store test artifacts (including screenshots but unlike `--screenshot-dir`, `dir` here is always a local directory regardless of platform and `--save-screenshots` isn't implied).
* `--verbose`: Print extra timing information.
* `--warmup-trials x`: Number of times to warm up the test before starting timing. Defaults to 3.
* `--warmup-steps x`: Maximum number of steps for the warmup loops. Defaults to unlimited.
* `--no-warmup`: Skip warming up the tests. Equivalent to `--warmup-steps 0`.
* `--calibration-time`: Run each test calibration step in a fixed time. Defaults to 1 second.
* `--trial-time x` or `--max-trial-time x`: Run each test trial under this max time. Defaults to 10 seconds.
* `--fixed-test-time x`: Run the tests until this much time has elapsed.
* `--trials`: Number of times to repeat testing. Defaults to 3.
* `--no-finish`: Don't call glFinish after each test trial.
* `--validation`: Enable serialization validation in the trace tests. Normally used with SwiftShader and retracing.
* `--perf-counters`: Additional performance counters to include in the result output. Separate multiple entries with colons: ':'.

The command line arguments implementations are located in [`ANGLEPerfTestArgs.cpp`](ANGLEPerfTestArgs.cpp).

## Test Breakdown

### Microbenchmarks

* [`DrawCallPerfBenchmark`](DrawCallPerf.cpp): Runs a tight loop around DrawArarys calls.
  * `validation_only`: Skips all rendering.
  * `render_to_texture`: Render to a user Framebuffer instead of the default FBO.
  * `vbo_change`: Applies a Vertex Array change between each draw.
  * `tex_change`: Applies a Texture change between each draw.
* [`UniformsBenchmark`](UniformsPerf.cpp): Tests performance of updating various uniforms counts followed by a DrawArrays call.
    * `vec4`: Tests `vec4` Uniforms.
    * `matrix`: Tests using Matrix uniforms instead of `vec4`.
    * `multiprogram`: Tests switching Programs between updates and draws.
    * `repeating`: Skip the update of uniforms before each draw call.
* [`DrawElementsPerfBenchmark`](DrawElementsPerf.cpp): Similar to `DrawCallPerfBenchmark` but for indexed DrawElements calls.
* [`BindingsBenchmark`](BindingPerf.cpp): Tests Buffer binding performance. Does no draw call operations.
    * `100_objects_allocated_every_iteration`: Tests repeated glBindBuffer with new buffers allocated each iteration.
    * `100_objects_allocated_at_initialization`: Tests repeated glBindBuffer the same objects each iteration.
* [`TexSubImageBenchmark`](TexSubImage.cpp): Tests `glTexSubImage` update performance.
* [`BufferSubDataBenchmark`](BufferSubData.cpp): Tests `glBufferSubData` update performance.
* [`TextureSamplingBenchmark`](TextureSampling.cpp): Tests Texture sampling performance.
* [`TextureBenchmark`](TexturesPerf.cpp): Tests Texture state change performance.
* [`LinkProgramBenchmark`](LinkProgramPerfTest.cpp): Tests performance of `glLinkProgram`.
* [`glmark2`](glmark2.cpp): Runs the glmark2 benchmark.

Many other tests can be found that have documentation in their classes.

### Trace Tests

* [`TracePerfTest`](TracePerfTest.cpp): Runs replays of restricted traces, not
  available publicly. To enable, read more in [`RestrictedTraceTests`](../restricted_traces/README.md)

Trace tests take command line arguments that pick the run configuration:

* `--use-gl=native`: Runs the tests against the default system GLES implementation instad of your local ANGLE.
* `--use-angle=backend`: Picks an ANGLE back-end. e.g. vulkan, d3d11, d3d9, gl, gles, metal, or swiftshader. Vulkan is the default.
* `--offscreen`: Run with an offscreen surface instead of swapping every frame.
* `--vsync`: Run with vsync enabled, and measure CPU and GPU work insead of wall clock time.
* `--minimize-gpu-work`: Modify API calls so that GPU work is reduced to minimum.
* `--screenshot-dir dir`: Directory to store test screenshots. Implies `--save-screenshots`. On Android this directory is on device, not local (see also `--render-test-output-dir`). Only implemented in `TracePerfTest`.
* `--save-screenshots`: Save screenshots. Only implemented in `TracePerfTest`.
* `--screenshot-frame <frame>`: Which frame to capture a screenshot of. Defaults to first frame (1). Only implemented in `TracePerfTest`.

For example, for an endless run with no warmup on swiftshader, run:

`angle_trace_tests --gtest_filter=TraceTest.trex_200 --use-angle=swiftshader --steps 1000000 --no-warmup`

## Understanding the Metrics

* `cpu_time`: Amount of CPU time consumed by an iteration of the test. This is backed by
`GetProcessTimes` on Windows, `getrusage` on Linux/Android, and `zx_object_get_info` on Fuchsia.
  * This value may sometimes be larger than `wall_time`. That is because we are summing up the time
on all CPU threads for the test.
* `wall_time`: Wall time taken to run a single iteration, calculated by dividing the total wall
clock time by the number of test iterations.
  * For trace tests, each rendered frame is an iteration.
* `gpu_time`: Estimated GPU elapsed time per test iteration. We compute the estimate using GLES
[timestamp queries](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt)
at the beginning and ending of each test loop.
  * For trace tests, this metric is only enabled in `vsync` mode.
