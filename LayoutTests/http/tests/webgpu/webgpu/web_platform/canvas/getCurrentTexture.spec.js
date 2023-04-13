/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for GPUCanvasContext.getCurrentTexture.
`;
import { SkipTestCase } from '../../../common/framework/fixture.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { assert, unreachable } from '../../../common/util/util.js';
import { GPUTest } from '../../gpu_test.js';
import { kAllCanvasTypes, createCanvas } from '../../util/create_elements.js';

class GPUContextTest extends GPUTest {
  initCanvasContext(canvasType = 'onscreen') {
    const canvas = createCanvas(this, canvasType, 2, 2);
    const ctx = canvas.getContext('webgpu');
    assert(ctx instanceof GPUCanvasContext, 'Failed to get WebGPU context from canvas');

    ctx.configure({
      device: this.device,
      format: 'rgba8unorm',
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
    });

    return ctx;
  }
}

export const g = makeTestGroup(GPUContextTest);

g.test('configured')
  .desc(
    `Checks that calling getCurrentTexture requires the context to be configured first, and
  that each call to configure causes getCurrentTexture to return a new texture.`
  )
  .params(u =>
    u //
      .combine('canvasType', kAllCanvasTypes)
  )
  .fn(t => {
    const canvas = createCanvas(t, t.params.canvasType, 2, 2);
    const ctx = canvas.getContext('webgpu');
    assert(ctx instanceof GPUCanvasContext, 'Failed to get WebGPU context from canvas');

    // Calling getCurrentTexture prior to configuration should throw an exception.
    t.shouldThrow(true, () => {
      ctx.getCurrentTexture();
    });

    // Once the context has been configured getCurrentTexture can be called.
    ctx.configure({
      device: t.device,
      format: 'rgba8unorm',
    });

    let prevTexture = ctx.getCurrentTexture();

    // Calling configure again with different values will change the texture returned.
    ctx.configure({
      device: t.device,
      format: 'bgra8unorm',
    });

    let currentTexture = ctx.getCurrentTexture();
    t.expect(prevTexture !== currentTexture);
    prevTexture = currentTexture;

    // Calling configure again with the same values will still change the texture returned.
    ctx.configure({
      device: t.device,
      format: 'bgra8unorm',
    });

    currentTexture = ctx.getCurrentTexture();
    t.expect(prevTexture !== currentTexture);
    prevTexture = currentTexture;

    // Calling getCurrentTexture after calling unconfigure should throw an exception.
    ctx.unconfigure();

    t.shouldThrow(true, () => {
      ctx.getCurrentTexture();
    });
  });

g.test('single_frames')
  .desc(`Checks that the value of getCurrentTexture is consistent within a single frame.`)
  .params(u =>
    u //
      .combine('canvasType', kAllCanvasTypes)
  )
  .fn(t => {
    const ctx = t.initCanvasContext(t.params.canvasType);
    const frameTexture = ctx.getCurrentTexture();

    // Calling getCurrentTexture a second time returns the same texture.
    t.expect(frameTexture === ctx.getCurrentTexture());

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: frameTexture.createView(),
          clearValue: [1.0, 0.0, 0.0, 1.0],
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    });
    pass.end();
    t.device.queue.submit([encoder.finish()]);

    // Calling getCurrentTexture after performing some work on the texture returns the same texture.
    t.expect(frameTexture === ctx.getCurrentTexture());

    // Ensure that getCurrentTexture does not clear the texture.
    t.expectSingleColor(frameTexture, frameTexture.format, {
      size: [frameTexture.width, frameTexture.height, 1],
      exp: { R: 1, G: 0, B: 0, A: 1 },
    });

    frameTexture.destroy();

    // Calling getCurrentTexture after destroying the texture still returns the same texture.
    t.expect(frameTexture === ctx.getCurrentTexture());
  });

g.test('multiple_frames')
  .desc(`Checks that the value of getCurrentTexture differs across multiple frames.`)
  .params(u =>
    u //
      .combine('canvasType', kAllCanvasTypes)
      .beginSubcases()
      .combine('clearTexture', [true, false])
  )
  .beforeAllSubcases(t => {
    const { canvasType } = t.params;
    if (canvasType === 'offscreen' && !('transferToImageBitmap' in OffscreenCanvas.prototype)) {
      throw new SkipTestCase('transferToImageBitmap not supported');
    }
  })
  .fn(t => {
    const { canvasType, clearTexture } = t.params;

    return new Promise(resolve => {
      const ctx = t.initCanvasContext(canvasType);
      let prevTexture;
      let frameCount = 0;

      function frameCheck() {
        const currentTexture = ctx.getCurrentTexture();

        if (prevTexture) {
          // Ensure that each frame a new texture object is returned.
          t.expect(currentTexture !== prevTexture);

          // Ensure that texture contents are transparent black.
          t.expectSingleColor(currentTexture, currentTexture.format, {
            size: [currentTexture.width, currentTexture.height, 1],
            exp: { R: 0, G: 0, B: 0, A: 0 },
          });
        }

        if (clearTexture) {
          // Clear the texture to test that texture contents don't carry over from frame to frame.
          const encoder = t.device.createCommandEncoder();
          const pass = encoder.beginRenderPass({
            colorAttachments: [
              {
                view: currentTexture.createView(),
                clearValue: [1.0, 0.0, 0.0, 1.0],
                loadOp: 'clear',
                storeOp: 'store',
              },
            ],
          });
          pass.end();
          t.device.queue.submit([encoder.finish()]);
        }

        prevTexture = currentTexture;

        if (frameCount++ < 5) {
          // Which method will be used to begin a new "frame"?
          switch (canvasType) {
            case 'onscreen':
              requestAnimationFrame(frameCheck);
              break;
            case 'offscreen': {
              ctx.canvas.transferToImageBitmap();
              // The beginning of frameCheck runs immediately (in the same task), so this
              // verifies the state has changed synchronously.
              void frameCheck();
              break;
            }
            default:
              unreachable();
          }
        } else {
          resolve();
        }
      }

      void frameCheck();
    });
  });

g.test('resize')
  .desc(`Checks the value of getCurrentTexture differs when the canvas is resized.`)
  .params(u =>
    u //
      .combine('canvasType', kAllCanvasTypes)
  )
  .fn(t => {
    const ctx = t.initCanvasContext(t.params.canvasType);
    let prevTexture = ctx.getCurrentTexture();

    // Trigger a resize by changing the width.
    ctx.canvas.width = 4;

    // When the canvas resizes the texture returned by getCurrentTexture should immediately begin
    // returning a new texture matching the update dimensions.
    let currentTexture = ctx.getCurrentTexture();
    t.expect(prevTexture !== currentTexture);
    t.expect(currentTexture.width === ctx.canvas.width);
    t.expect(currentTexture.height === ctx.canvas.height);

    // The width and height of the previous texture should remain unchanged.
    t.expect(prevTexture.width === 2);
    t.expect(prevTexture.height === 2);
    prevTexture = currentTexture;

    // Ensure that texture contents are transparent black.
    t.expectSingleColor(currentTexture, currentTexture.format, {
      size: [currentTexture.width, currentTexture.height, 1],
      exp: { R: 0, G: 0, B: 0, A: 0 },
    });

    // Trigger a resize by changing the height.
    ctx.canvas.height = 4;

    // Check to ensure the texture is resized again.
    currentTexture = ctx.getCurrentTexture();
    t.expect(prevTexture !== currentTexture);
    t.expect(currentTexture.width === ctx.canvas.width);
    t.expect(currentTexture.height === ctx.canvas.height);
    t.expect(prevTexture.width === 4);
    t.expect(prevTexture.height === 2);
    prevTexture = currentTexture;

    // Ensure that texture contents are transparent black.
    t.expectSingleColor(currentTexture, currentTexture.format, {
      size: [currentTexture.width, currentTexture.height, 1],
      exp: { R: 0, G: 0, B: 0, A: 0 },
    });

    // Simply setting the canvas width and height values to their current values should not trigger
    // a change in the texture.
    ctx.canvas.width = 4;
    ctx.canvas.height = 4;

    currentTexture = ctx.getCurrentTexture();
    t.expect(prevTexture === currentTexture);
  });
