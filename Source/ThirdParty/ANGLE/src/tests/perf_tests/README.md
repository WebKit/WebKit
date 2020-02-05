# ANGLE Performance Tests

`angle_perftests` is a standalone testing suite that contains targeted tests for OpenGL, Vulkan and ANGLE internal classes. The tests currently run on the Chromium ANGLE infrastructure and report results to the [Chromium perf dashboard](https://chromeperf.appspot.com/report).

You can also build your own dashboards. For example, a comparison of ANGLE's back-end draw call performance on Windows NVIDIA can be found [at this link](https://chromeperf.appspot.com/report?sid=1fdf94a308f52b6bf02c08f6f36e87ca0d0075e2d2eefc61e6cf90c919c1643a&start_rev=577814&end_rev=582136). Note that this link is not kept current.

## Running the Tests

You can follow the usual instructions to [check out and build ANGLE](../../../doc/DevSetup.md). Build the `angle_perftests` target. Note that all test scores are higher-is-better. You should also ensure `is_debug=false` in your build. Running with `dcheck_always_on` or debug validation enabled is not recommended.

Variance can be a problem when benchmarking. We have a test harness to run a single test in an infinite loop and print some statistics to help mitigate variance. See [`scripts/perf_test_runner.py`](https://chromium.googlesource.com/angle/angle/+/master/scripts/perf_test_runner.py). To use the script first compile `angle_perftests` into a folder with the word `Release` in it. Then provide the name of the test as the argument to the script. The script will automatically pick up the most current `angle_perftests` and run in an infinite loop.

### Choosing the Test to Run

You can choose individual tests to run with `--gtest_filter=*TestName*`. To select a particular ANGLE back-end, add the name of the back-end to the test filter. For example: `DrawCallPerfBenchmark.Run/gl` or `DrawCallPerfBenchmark.Run/d3d11`. Many tests have sub-tests that run slightly different code paths. You might need to experiment to find the right sub-test and its name.

### Null/No-op Configurations

ANGLE implements a no-op driver for OpenGL, D3D11 and Vulkan. To run on these configurations use the `gl_null`, `d3d11_null` or `vulkan_null` test configurations. These null drivers will not do any GPU work. They will skip the driver entirely. These null configs are useful for diagnosing performance overhead in ANGLE code.

## Test Breakdown

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
* [`TracePerfTest`](TracePerfTest.cpp): Runs replays of restricted traces, not available publicly. To enable, read more in [`RestrictedTraceTests`](restricted_traces/RestrictedTraceTests.md)

Many other tests can be found that have documentation in their classes.
