/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `API Operation Tests for RenderPass StoreOp.
Tests a render pass with a resolveTarget resolves correctly for many combinations of:
  - number of color attachments, some with and some without a resolveTarget
  - renderPass storeOp set to {'store', 'discard'}
  - resolveTarget mip level {0, >0} (TODO?: different mip level from colorAttachment)
  - resolveTarget {2d array layer, TODO: 3d slice} {0, >0} with {2d, TODO: 3d} resolveTarget
    (TODO?: different z from colorAttachment)
  - TODO: test all renderable color formats
  - TODO: test that any not-resolved attachments are rendered to correctly.
  - TODO: test different loadOps
  - TODO?: resolveTarget mip level {0, >0} (TODO?: different mip level from colorAttachment)
  - TODO?: resolveTarget {2d array layer, TODO: 3d slice} {0, >0} with {2d, TODO: 3d} resolveTarget
    (different z from colorAttachment)
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

const kSlotsToResolve = [
  [0, 2],
  [1, 3],
  [0, 1, 2, 3],
];

const kSize = 4;
const kFormat = 'rgba8unorm';

export const g = makeTestGroup(GPUTest);

g.test('render_pass_resolve')
  .params(u =>
    u
      .combine('storeOperation', ['discard', 'store'])
      .beginSubcases()
      .combine('numColorAttachments', [2, 4])
      .combine('slotsToResolve', kSlotsToResolve)
      .combine('resolveTargetBaseMipLevel', [0, 1])
      .combine('resolveTargetBaseArrayLayer', [0, 1])
  )
  .fn(t => {
    const targets = [];
    for (let i = 0; i < t.params.numColorAttachments; i++) {
      targets.push({ format: kFormat });
    }

    // These shaders will draw a white triangle into a texture. After draw, the top left
    // half of the texture will be white, and the bottom right half will be unchanged. When this
    // texture is resolved, there will be two distinct colors in each portion of the texture, as
    // well as a line between the portions that contain the midpoint color due to the multisample
    // resolve.
    const pipeline = t.device.createRenderPipeline({
      vertex: {
        module: t.device.createShaderModule({
          code: `
            @stage(vertex) fn main(
              @builtin(vertex_index) VertexIndex : u32
              ) -> @builtin(position) vec4<f32> {
              var pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                  vec2<f32>(-1.0, -1.0),
                  vec2<f32>(-1.0,  1.0),
                  vec2<f32>( 1.0,  1.0));
              return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
            }`,
        }),

        entryPoint: 'main',
      },

      fragment: {
        module: t.device.createShaderModule({
          code: `
            struct Output {
              @location(0) fragColor0 : vec4<f32>,
              @location(1) fragColor1 : vec4<f32>,
              @location(2) fragColor2 : vec4<f32>,
              @location(3) fragColor3 : vec4<f32>,
            };

            @stage(fragment) fn main() -> Output {
              return Output(
                vec4<f32>(1.0, 1.0, 1.0, 1.0),
                vec4<f32>(1.0, 1.0, 1.0, 1.0),
                vec4<f32>(1.0, 1.0, 1.0, 1.0),
                vec4<f32>(1.0, 1.0, 1.0, 1.0)
              );
            }`,
        }),

        entryPoint: 'main',
        targets,
      },

      primitive: { topology: 'triangle-list' },
      multisample: { count: 4 },
    });

    const resolveTargets = [];
    const renderPassColorAttachments = [];

    // The resolve target must be the same size as the color attachment. If we're resolving to mip
    // level 1, the resolve target base mip level should be 2x the color attachment size.
    const kResolveTargetSize = kSize << t.params.resolveTargetBaseMipLevel;

    for (let i = 0; i < t.params.numColorAttachments; i++) {
      const colorAttachment = t.device.createTexture({
        format: kFormat,
        size: { width: kSize, height: kSize, depthOrArrayLayers: 1 },
        sampleCount: 4,
        mipLevelCount: 1,
        usage:
          GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
      });

      if (t.params.slotsToResolve.includes(i)) {
        const colorAttachment = t.device.createTexture({
          format: kFormat,
          size: { width: kSize, height: kSize, depthOrArrayLayers: 1 },
          sampleCount: 4,
          mipLevelCount: 1,
          usage:
            GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
        });

        const resolveTarget = t.device.createTexture({
          format: kFormat,
          size: {
            width: kResolveTargetSize,
            height: kResolveTargetSize,
            depthOrArrayLayers: t.params.resolveTargetBaseArrayLayer + 1,
          },

          sampleCount: 1,
          mipLevelCount: t.params.resolveTargetBaseMipLevel + 1,
          usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
        });

        // Clear to black for the load operation. After the draw, the top left half of the attachment
        // will be white and the bottom right half will be black.
        renderPassColorAttachments.push({
          view: colorAttachment.createView(),
          clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
          loadOp: 'clear',
          storeOp: t.params.storeOperation,
          resolveTarget: resolveTarget.createView({
            baseMipLevel: t.params.resolveTargetBaseMipLevel,
            baseArrayLayer: t.params.resolveTargetBaseArrayLayer,
          }),
        });

        resolveTargets.push(resolveTarget);
      } else {
        renderPassColorAttachments.push({
          view: colorAttachment.createView(),
          clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
          loadOp: 'clear',
          storeOp: t.params.storeOperation,
        });
      }
    }

    const encoder = t.device.createCommandEncoder();

    const pass = encoder.beginRenderPass({
      colorAttachments: renderPassColorAttachments,
    });

    pass.setPipeline(pipeline);
    pass.draw(3);
    pass.end();
    t.device.queue.submit([encoder.finish()]);

    // Verify the resolve targets contain the correct values.
    for (const resolveTarget of resolveTargets) {
      // Test top left pixel, which should be {255, 255, 255, 255}.
      t.expectSinglePixelIn2DTexture(
        resolveTarget,
        kFormat,
        { x: 0, y: 0 },
        {
          exp: new Uint8Array([0xff, 0xff, 0xff, 0xff]),
          slice: t.params.resolveTargetBaseArrayLayer,
          layout: { mipLevel: t.params.resolveTargetBaseMipLevel },
        }
      );

      // Test bottom right pixel, which should be {0, 0, 0, 0}.
      t.expectSinglePixelIn2DTexture(
        resolveTarget,
        kFormat,
        { x: kSize - 1, y: kSize - 1 },
        {
          exp: new Uint8Array([0x00, 0x00, 0x00, 0x00]),
          slice: t.params.resolveTargetBaseArrayLayer,
          layout: { mipLevel: t.params.resolveTargetBaseMipLevel },
        }
      );

      // Test top right pixel, which should be {127, 127, 127, 127} due to the multisampled resolve.
      t.expectSinglePixelBetweenTwoValuesIn2DTexture(
        resolveTarget,
        kFormat,
        { x: kSize - 1, y: 0 },
        {
          exp: [new Uint8Array([0x7f, 0x7f, 0x7f, 0x7f]), new Uint8Array([0x80, 0x80, 0x80, 0x80])],
          slice: t.params.resolveTargetBaseArrayLayer,
          layout: { mipLevel: t.params.resolveTargetBaseMipLevel },
        }
      );
    }
  });
