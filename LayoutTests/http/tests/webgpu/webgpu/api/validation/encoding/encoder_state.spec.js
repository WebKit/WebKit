/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- createCommandEncoder
- non-pass command, or beginPass, during {render, compute} pass
- {before (control case), after} finish()
    - x= {finish(), ... all non-pass commands}
- {before (control case), after} end()
    - x= {render, compute} pass
    - x= {finish(), ... all relevant pass commands}
    - x= {
        - before endPass (control case)
        - after endPass (no pass open)
        - after endPass+beginPass (a new pass of the same type is open)
        - }
    - should make whole encoder invalid
- ?
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { objectEquals } from '../../../../common/util/util.js';
import { ValidationTest } from '../validation_test.js';

class F extends ValidationTest {
  beginRenderPass(commandEncoder, view) {
    return commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          view,
          clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    });
  }

  createAttachmentTextureView() {
    const texture = this.device.createTexture({
      format: 'rgba8unorm',
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });
    this.trackForCleanup(texture);
    return texture.createView();
  }
}

export const g = makeTestGroup(F);

g.test('pass_end_invalid_order')
  .desc(
    `
  Test that beginning a {compute,render} pass before ending the previous {compute,render} pass
  causes an error.

  TODO: Update this test according to https://github.com/gpuweb/gpuweb/issues/2464
  `
  )
  .params(u =>
    u
      .combine('pass0Type', ['compute', 'render'])
      .combine('pass1Type', ['compute', 'render'])
      .beginSubcases()
      .combine('firstPassEnd', [true, false])
      .combine('endPasses', [[], [0], [1], [0, 1], [1, 0]])
  )
  .fn(t => {
    const { pass0Type, pass1Type, firstPassEnd, endPasses } = t.params;

    const view = t.createAttachmentTextureView();
    const encoder = t.device.createCommandEncoder();

    const firstPass =
      pass0Type === 'compute' ? encoder.beginComputePass() : t.beginRenderPass(encoder, view);

    if (firstPassEnd) firstPass.end();

    // Begin a second pass before ending the previous pass.
    const secondPass =
      pass1Type === 'compute' ? encoder.beginComputePass() : t.beginRenderPass(encoder, view);

    const passes = [firstPass, secondPass];
    for (const index of endPasses) {
      passes[index].end();
    }

    // If {endPasses} is '[1]' and {firstPass} ends, it's a control case.
    const valid = firstPassEnd && objectEquals(endPasses, [1]);

    t.expectValidationError(() => {
      encoder.finish();
    }, !valid);
  });

g.test('call_after_successful_finish')
  .desc(`Test that encoding command after a successful finish generates a validation error.`)
  .params(u =>
    u
      .combine('callCmd', ['beginComputePass', 'beginRenderPass', 'insertDebugMarker'])
      .beginSubcases()
      .combine('prePassType', ['compute', 'render', 'no-op'])
      .combine('IsEncoderFinished', [false, true])
  )
  .fn(t => {
    const { prePassType, IsEncoderFinished, callCmd } = t.params;

    const view = t.createAttachmentTextureView();
    const encoder = t.device.createCommandEncoder();

    if (prePassType !== 'no-op') {
      const pass =
        prePassType === 'compute' ? encoder.beginComputePass() : t.beginRenderPass(encoder, view);
      pass.end();
    }

    if (IsEncoderFinished) {
      encoder.finish();
    }

    switch (callCmd) {
      case 'beginComputePass':
        {
          let pass;
          t.expectValidationError(() => {
            pass = encoder.beginComputePass();
          }, IsEncoderFinished);
          t.expectValidationError(() => {
            pass.end();
          }, IsEncoderFinished);
        }
        break;
      case 'beginRenderPass':
        {
          let pass;
          t.expectValidationError(() => {
            pass = t.beginRenderPass(encoder, view);
          }, IsEncoderFinished);
          t.expectValidationError(() => {
            pass.end();
          }, IsEncoderFinished);
        }
        break;
      case 'insertDebugMarker':
        t.expectValidationError(() => {
          encoder.insertDebugMarker('');
        }, IsEncoderFinished);
        break;
    }

    if (!IsEncoderFinished) {
      encoder.finish();
    }
  });

g.test('pass_end_none')
  .desc(
    `
  Test that ending a {compute,render} pass without ending the passes generates a validation error.
  `
  )
  .paramsSubcasesOnly(u => u.combine('passType', ['compute', 'render']).combine('endCount', [0, 1]))
  .fn(t => {
    const { passType, endCount } = t.params;

    const view = t.createAttachmentTextureView();
    const encoder = t.device.createCommandEncoder();

    const pass =
      passType === 'compute' ? encoder.beginComputePass() : t.beginRenderPass(encoder, view);

    for (let i = 0; i < endCount; ++i) {
      pass.end();
    }

    t.expectValidationError(() => {
      encoder.finish();
    }, endCount === 0);
  });

g.test('pass_end_twice')
  .desc('Test that ending a {compute,render} pass twice generates a validation error.')
  .paramsSubcasesOnly(u =>
    u //
      .combine('passType', ['compute', 'render'])
      .combine('endTwice', [false, true])
  )
  .fn(t => {
    const { passType, endTwice } = t.params;

    const view = t.createAttachmentTextureView();
    const encoder = t.device.createCommandEncoder();

    const pass =
      passType === 'compute' ? encoder.beginComputePass() : t.beginRenderPass(encoder, view);

    pass.end();
    if (endTwice) {
      t.expectValidationError(() => {
        pass.end();
      });
    }

    encoder.finish();
  });
