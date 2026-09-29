/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Atomically stores the min value v in the atomic object pointed to by atomic_ptr.
`;import { makeTestGroup } from '../../../../../../../common/framework/test_group.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../../../../gpu_test.js';
import { kValue } from '../../../../../../util/constants.js';

import { dispatchSizes, onlyWorkgroupSizes } from './harness.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);

g.test('store_min_basic').
specURL('https://www.w3.org/TR/WGSL/#atomic-store-min').
desc(
  `
AS is storage
T is vec2u

fn atomicStoreMin(atomic_ptr: ptr<AS, atomic<T>, read_write>, v: T)

Tests that multiple invocations of atomicStoreMin to the same location returns
the composite min of all values written.
`
).
params((u) =>
u.
combine('workgroupSize', onlyWorkgroupSizes).
combine('dispatchSize', dispatchSizes).
combine('rndMultiplyX', [0, 1, 3163, 176284061, 2 ** 28]).
combine('rndMultiplyY', [0, 1, 41609, 138545483, 2 ** 28])
).
fn(async (t) => {
  t.skipIfDeviceDoesNotHaveFeature('atomic-vec2u-min-max');
  const wgsl = `
      enable atomic_vec2u_min_max;
      @group(0) @binding(0)
      var<storage, read_write> output : atomic<vec2u>;

      @compute @workgroup_size(${t.params.workgroupSize})
      fn main(
          @builtin(global_invocation_id) global_invocation_id : vec3<u32>,
          ) {
          let reversal = vec2u(${t.params.workgroupSize} - global_invocation_id[0],
                               ${t.params.dispatchSize} - global_invocation_id[1]);
        let id = reversal * vec2u(  ${t.params.rndMultiplyX} , ${t.params.rndMultiplyY});

        // All invocations store to the same location
        atomicStoreMin(&output, id);
      }
    `;

  const pipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: {
      module: t.device.createShaderModule({ code: wgsl }),
      entryPoint: 'main'
    }
  });

  // For a single vec2u atomic we need 2 u32s.
  // Initialize with max u32 value.
  const data = new Uint32Array([kValue.u32.max, kValue.u32.max]);
  const outputBuffer = t.makeBufferWithContents(
    data,
    GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  );

  const bindGroup = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [{ binding: 0, resource: { buffer: outputBuffer } }]
  });

  // Run the shader.
  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bindGroup);
  // Dispatch will be Y for simplicity.
  pass.dispatchWorkgroups(1, t.params.dispatchSize);
  pass.end();
  t.queue.submit([encoder.finish()]);

  // Read back the buffer
  const outputBufferResult = (
  await t.readGPUBufferRangeTyped(outputBuffer, {
    type: Uint32Array,
    typedLength: outputBuffer.size / Uint32Array.BYTES_PER_ELEMENT
  })).
  data;

  let expectedMinX = Number(kValue.u32.max);
  let expectedMinY = Number(kValue.u32.max);

  // Simulate shader execution to find the max
  for (let x = 0; x < t.params.workgroupSize; x++) {
    for (let y = 0; y < t.params.dispatchSize; y++) {
      const x_reversal = t.params.workgroupSize - x;
      const y_reversal = t.params.dispatchSize - y;
      const idX = t.params.rndMultiplyX * x_reversal >>> 0;
      const idY = t.params.rndMultiplyY * y_reversal >>> 0;

      if (idY < expectedMinY || idY === expectedMinY && idX < expectedMinX) {
        expectedMinX = idX;
        expectedMinY = idY;
      }
    }
  }

  if (outputBufferResult[0] !== expectedMinX || outputBufferResult[1] !== expectedMinY) {
    t.fail(
      `Unexpected value in output: [${outputBufferResult[0]}, ${outputBufferResult[1]}], expected: [${expectedMinX}, ${expectedMinY}]`
    );
  }
});