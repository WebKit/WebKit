/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ // WPT-specific test checking that WebGPU is available iff isSecureContext.
import { assert } from '../../common/util/util.js';
const items = [
  globalThis.navigator.gpu,
  globalThis.GPU,
  globalThis.GPUAdapter,
  globalThis.GPUDevice,
  globalThis.GPUBuffer,
  globalThis.GPUBufferUsage,
  globalThis.GPUCommandEncoder,
  globalThis.GPUCommandBuffer,
  globalThis.GPUComputePassEncoder,
  globalThis.GPURenderPipeline,
  globalThis.GPUDeviceLostInfo,
  globalThis.GPUValidationError,
  // Need to test the rest of the interfaces.
];

for (const item of items) {
  if (globalThis.isSecureContext) {
    assert(item !== undefined, 'Item/interface should be exposed on secure context');
  } else {
    assert(item === undefined, 'Item/interface should not be exposed on insecure context');
  }
}
