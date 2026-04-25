/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
API operations tests for timestamp queries.

Given the values returned are implementation defined
there is not much we can test except that there are no errors.

- test query with
  - compute pass
  - render pass
  - 64k query objects
`;import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../../gpu_test.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);

g.test('many_query_sets').
desc(
  `
Test creating and using 64k query objects.

This test is because there is a Metal limit of 32 MTLCounterSampleBuffers
Implementations are supposed to work around this limit by internally allocating
larger MTLCounterSampleBuffers and having the WebGPU sets be subsets of those
larger buffers.

This is particular important as the limit is 32 per process
so a few pages making a few queries would easily hit the limit
and prevent pages from running.
    `
).
params((u) =>
u.
combine('numQuerySets', [8, 16, 32, 64, 256, 65536]).
combine('stage', ['compute', 'render'])
).
fn(async (t) => {
  const { stage, numQuerySets } = t.params;

  t.skipIfDeviceDoesNotHaveFeature('timestamp-query');

  // At large numQuerySets, this test incurs a lot of validation, which can take several seconds.
  // Explicitly wrap the test in its own error scope to avoid triggering timeouts in test-cleanup.
  t.device.pushErrorScope('validation');
  try {
    const view = t.
    createTextureTracked({
      size: [1, 1, 1],
      format: 'rgba8unorm',
      usage: GPUTextureUsage.RENDER_ATTACHMENT
    }).
    createView();
    const encoder = t.device.createCommandEncoder();

    for (let i = 0; i < numQuerySets; ++i) {
      const querySet = t.createQuerySetTracked({
        type: 'timestamp',
        count: 2
      });

      switch (stage) {
        case 'compute':{
            const pass = encoder.beginComputePass({
              timestampWrites: {
                querySet,
                beginningOfPassWriteIndex: 0,
                endOfPassWriteIndex: 1
              }
            });
            pass.end();
            break;
          }
        case 'render':{
            const pass = encoder.beginRenderPass({
              colorAttachments: [{ view, loadOp: 'load', storeOp: 'store' }],
              timestampWrites: {
                querySet,
                beginningOfPassWriteIndex: 0,
                endOfPassWriteIndex: 1
              }
            });
            pass.end();
            break;
          }
      }
    }

    const shouldError = false; // just expect no error
    t.expectValidationError(() => t.device.queue.submit([encoder.finish()]), shouldError);
  } finally {
    const error = await t.device.popErrorScope();
    // Make sure there weren't any unexpected validation errors caught by the scope.
    t.expect(error === null, error?.message);
  }
});

g.test('many_slots').
desc(
  `
Test creating and using 4k query slots.

Metal has the limit that a MTLCounterSampleBuffer can be max 32k which is 4k slots.
So, test we can use 4k slots across a few QuerySets
    `
).
params((u) => u.combine('stage', ['compute', 'render'])).
fn((t) => {
  const { stage } = t.params;

  t.skipIfDeviceDoesNotHaveFeature('timestamp-query');
  const kNumSlots = 4096;
  const kNumQuerySets = 4;

  const view = t.
  createTextureTracked({
    size: [1, 1, 1],
    format: 'rgba8unorm',
    usage: GPUTextureUsage.RENDER_ATTACHMENT
  }).
  createView();
  const encoder = t.device.createCommandEncoder();

  for (let i = 0; i < kNumQuerySets; ++i) {
    const querySet = t.createQuerySetTracked({
      type: 'timestamp',
      count: kNumSlots
    });

    switch (stage) {
      case 'compute':{
          for (let slot = 0; slot < kNumSlots; slot += 2) {
            const pass = encoder.beginComputePass({
              timestampWrites: {
                querySet,
                beginningOfPassWriteIndex: slot,
                endOfPassWriteIndex: slot + 1
              }
            });
            pass.end();
          }
          break;
        }
      case 'render':{
          for (let slot = 0; slot < kNumSlots; slot += 2) {
            const pass = encoder.beginRenderPass({
              colorAttachments: [{ view, loadOp: 'load', storeOp: 'store' }],
              timestampWrites: {
                querySet,
                beginningOfPassWriteIndex: slot,
                endOfPassWriteIndex: slot + 1
              }
            });
            pass.end();
          }
          break;
        }
    }
  }

  const shouldError = false; // just expect no error
  t.expectValidationError(() => t.device.queue.submit([encoder.finish()]), shouldError);
});