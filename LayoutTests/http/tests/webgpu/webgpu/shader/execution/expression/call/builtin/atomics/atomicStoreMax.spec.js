/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Atomically stores the max value v in the atomic object pointed to by atomic_ptr.
`;import { makeTestGroup } from '../../../../../../../common/framework/test_group.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../../../../gpu_test.js';

import { dispatchSizes, onlyWorkgroupSizes, typedArrayCtor } from './harness.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);

g.test('store_max_basic').
specURL('https://www.w3.org/TR/WGSL/#atomic-store-max').
desc(
  `
AS is storage
T is vec2u

fn atomicStoreMax(atomic_ptr: ptr<AS, atomic<T>, read_write>, v: T)

Tests that multiple invocations of atomicStoreMax to the same location returns
the composite max of all values written.
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
        let id = vec2u(global_invocation_id[0] * ${t.params.rndMultiplyX} ,
                       global_invocation_id[1] * ${t.params.rndMultiplyY});

        // All invocations store to the same location
        atomicStoreMax(&output, id);
      }
    `;

  const pipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: {
      module: t.device.createShaderModule({ code: wgsl }),
      entryPoint: 'main'
    }
  });

  const arrayType = typedArrayCtor('u32');

  // For a single atomic we need 2 u32s
  const outputBuffer = t.createBufferTracked({
    size: 2 * arrayType.BYTES_PER_ELEMENT,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

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
    type: arrayType,
    typedLength: outputBuffer.size / arrayType.BYTES_PER_ELEMENT
  })).
  data;

  let expectedMaxX = 0;
  let expectedMaxY = 0;

  // Simulate shader execution to find the max
  for (let x = 0; x < t.params.workgroupSize; x++) {
    for (let y = 0; y < t.params.dispatchSize; y++) {
      const idX = t.params.rndMultiplyX * x >>> 0;
      const idY = t.params.rndMultiplyY * y >>> 0;

      if (idY > expectedMaxY || idY === expectedMaxY && idX > expectedMaxX) {
        expectedMaxX = idX;
        expectedMaxY = idY;
      }
    }
  }

  if (outputBufferResult[0] !== expectedMaxX || outputBufferResult[1] !== expectedMaxY) {
    t.fail(
      `Unexpected value in output: [${outputBufferResult[0]}, ${outputBufferResult[1]}], expected: [${expectedMaxX}, ${expectedMaxY}]`
    );
  }
});