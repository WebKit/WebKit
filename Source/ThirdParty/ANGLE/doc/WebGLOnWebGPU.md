# WebGL on WebGPU

> **Status:** Work in progress. Development on the ANGLE backend and the Blink integration is underway.

WebGLOnWebGPU adds a WebGPU backend to ANGLE. Chrome can then convert WebGL calls into WebGPU calls inside the renderer process and leave the rest to Dawn, which shrinks the GPU process attack surface.

## Source

| Component        | Repo     | Location                                                                         |
| ---------------- | -------- | -------------------------------------------------------------------------------- |
| ANGLE backend    | ANGLE    | [`src/libANGLE/renderer/wgpu/`](../src/libANGLE/renderer/wgpu/)                  |
| Compiler backend | ANGLE    | [`src/compiler/translator/wgsl/`](../src/compiler/translator/wgsl/)              |
| Chrome bindings  | Chromium | [`third_party/blink/renderer/modules/webgl/webgl_rendering_context_webgpu_base.cc`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/webgl/webgl_rendering_context_webgpu_base.cc) |

## Build

The backend builds by default on most platforms. To turn it on explicitly, add this to `args.gn`:

```gn
angle_enable_wgpu = true
```

## Testing

### angle_end2end_tests

```sh
autoninja -C out/Release angle_end2end_tests
out/Release/angle_end2end_tests --gtest_filter=*WebGPU*
```

Expectations: [`angle_end2end_tests_expectations.txt`](../src/tests/angle_end2end_tests_expectations.txt). Search for `WGPU` to find the WebGPU entries.

### dEQP

```sh
autoninja -C out/Release angle_deqp_gles2_tests angle_deqp_gles3_tests
out/Release/angle_deqp_gles2_tests --use-angle=webgpu
out/Release/angle_deqp_gles3_tests --use-angle=webgpu
```

Expectations: [`deqp_gles2_test_expectations.txt`](../src/tests/deqp_support/deqp_gles2_test_expectations.txt), [`deqp_gles3_test_expectations.txt`](../src/tests/deqp_support/deqp_gles3_test_expectations.txt) and the neighbouring files. Search for `WGPU` to find the WebGPU entries.

To look at the results, use Cherry, a web app that shows image diffs and test logs. See [dEQP.md#check-your-results](dEQP.md#check-your-results).

### WebGL CTS (Chrome)

No extra build flags are needed. Run Chrome with:

```sh
--enable-features=WebGLOnWebGPU
```

Navigate to the [WebGL conformance tests](https://registry.khronos.org/webgl/sdk/tests/webgl-conformance-tests.html).
