/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Test related to depth buffer, depth op, compare func, etc.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { kDepthStencilFormats, kTextureFormatInfo } from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';

const backgroundColor = [0x00, 0x00, 0x00, 0xff];
const triangleColor = [0xff, 0xff, 0xff, 0xff];

export const g = makeTestGroup(GPUTest);

g.test('depth_bias')
  .desc(
    `Tests render results with different depth bias values: positive, negative, infinity, slope, clamp, etc.`
  )
  .unimplemented();

g.test('depth_disabled').desc(`Tests render results with depth test disabled`).unimplemented();

g.test('depth_write_disabled')
  .desc(`Tests render results with depth write disabled`)
  .unimplemented();

// Use a depth value that's not exactly 0.5 because it is exactly between two depth16unorm value and
// can get rounded either way (and a different way between shaders and clearDepthValue).
const kMiddleDepthValue = 0.5001;

g.test('depth_compare_func')
  .desc(
    `Tests each depth compare function works properly. Clears the depth attachment to various values, and renders a point at depth 0.5 with various depthCompare modes.`
  )
  .params(u =>
    u
      .combine(
        'format',
        kDepthStencilFormats.filter(format => kTextureFormatInfo[format].depth)
      )
      .combineWithParams([
        { depthCompare: 'never', depthClearValue: 1.0, _expected: backgroundColor },
        { depthCompare: 'never', depthClearValue: kMiddleDepthValue, _expected: backgroundColor },
        { depthCompare: 'never', depthClearValue: 0.0, _expected: backgroundColor },
        { depthCompare: 'less', depthClearValue: 1.0, _expected: triangleColor },
        { depthCompare: 'less', depthClearValue: kMiddleDepthValue, _expected: backgroundColor },
        { depthCompare: 'less', depthClearValue: 0.0, _expected: backgroundColor },
        { depthCompare: 'less-equal', depthClearValue: 1.0, _expected: triangleColor },
        {
          depthCompare: 'less-equal',
          depthClearValue: kMiddleDepthValue,
          _expected: triangleColor,
        },

        { depthCompare: 'less-equal', depthClearValue: 0.0, _expected: backgroundColor },
        { depthCompare: 'equal', depthClearValue: 1.0, _expected: backgroundColor },
        { depthCompare: 'equal', depthClearValue: kMiddleDepthValue, _expected: triangleColor },
        { depthCompare: 'equal', depthClearValue: 0.0, _expected: backgroundColor },
        { depthCompare: 'not-equal', depthClearValue: 1.0, _expected: triangleColor },
        {
          depthCompare: 'not-equal',
          depthClearValue: kMiddleDepthValue,
          _expected: backgroundColor,
        },

        { depthCompare: 'not-equal', depthClearValue: 0.0, _expected: triangleColor },
        { depthCompare: 'greater-equal', depthClearValue: 1.0, _expected: backgroundColor },
        {
          depthCompare: 'greater-equal',
          depthClearValue: kMiddleDepthValue,
          _expected: triangleColor,
        },

        { depthCompare: 'greater-equal', depthClearValue: 0.0, _expected: triangleColor },
        { depthCompare: 'greater', depthClearValue: 1.0, _expected: backgroundColor },
        { depthCompare: 'greater', depthClearValue: kMiddleDepthValue, _expected: backgroundColor },
        { depthCompare: 'greater', depthClearValue: 0.0, _expected: triangleColor },
        { depthCompare: 'always', depthClearValue: 1.0, _expected: triangleColor },
        { depthCompare: 'always', depthClearValue: kMiddleDepthValue, _expected: triangleColor },
        { depthCompare: 'always', depthClearValue: 0.0, _expected: triangleColor },
      ])
  )
  .fn(async t => {
    const { depthCompare, depthClearValue, _expected, format } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(format);

    const colorAttachmentFormat = 'rgba8unorm';
    const colorAttachment = t.device.createTexture({
      format: colorAttachmentFormat,
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const colorAttachmentView = colorAttachment.createView();

    const depthTexture = t.device.createTexture({
      size: { width: 1, height: 1 },
      format,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
    });

    const depthTextureView = depthTexture.createView();

    const pipelineDescriptor = {
      vertex: {
        module: t.device.createShaderModule({
          code: `
            @stage(vertex) fn main(
              @builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
              return vec4<f32>(0.5, 0.5, ${kMiddleDepthValue}, 1.0);
            }
            `,
        }),

        entryPoint: 'main',
      },

      fragment: {
        module: t.device.createShaderModule({
          code: `
            @stage(fragment) fn main() -> @location(0) vec4<f32> {
              return vec4<f32>(1.0, 1.0, 1.0, 1.0);
            }
            `,
        }),

        entryPoint: 'main',
        targets: [{ format: colorAttachmentFormat }],
      },

      primitive: { topology: 'point-list' },
      depthStencil: {
        depthWriteEnabled: true,
        depthCompare,
        format,
      },
    };

    const pipeline = t.device.createRenderPipeline(pipelineDescriptor);

    const encoder = t.device.createCommandEncoder();
    const depthStencilAttachment = {
      view: depthTextureView,
      depthClearValue,
      depthLoadOp: 'clear',
      depthStoreOp: 'store',
    };

    if (kTextureFormatInfo[format].stencil) {
      depthStencilAttachment.stencilClearValue = 0;
      depthStencilAttachment.stencilLoadOp = 'clear';
      depthStencilAttachment.stencilStoreOp = 'store';
    }
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: colorAttachmentView,
          storeOp: 'store',
          clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
          loadOp: 'clear',
        },
      ],

      depthStencilAttachment,
    });

    pass.setPipeline(pipeline);
    pass.draw(1);
    pass.end();
    t.device.queue.submit([encoder.finish()]);

    t.expectSinglePixelIn2DTexture(
      colorAttachment,
      colorAttachmentFormat,
      { x: 0, y: 0 },
      { exp: new Uint8Array(_expected) }
    );
  });

g.test('reverse_depth')
  .desc(
    `Tests simple rendering with reversed depth buffer, ensures depth test works properly: fragments are in correct order and out of range fragments are clipped.
    Note that in real use case the depth range remapping is done by the modified projection matrix.
(see https://developer.nvidia.com/content/depth-precision-visualized).`
  )
  .params(u => u.combine('reversed', [false, true]))
  .fn(async t => {
    const colorAttachmentFormat = 'rgba8unorm';
    const colorAttachment = t.device.createTexture({
      format: colorAttachmentFormat,
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const colorAttachmentView = colorAttachment.createView();

    const depthBufferFormat = 'depth32float';
    const depthTexture = t.device.createTexture({
      size: { width: 1, height: 1 },
      format: depthBufferFormat,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
    });

    const depthTextureView = depthTexture.createView();

    const pipelineDescriptor = {
      vertex: {
        module: t.device.createShaderModule({
          code: `
            struct Output {
              @builtin(position) Position : vec4<f32>,
              @location(0) color : vec4<f32>,
            };

            @stage(vertex) fn main(
              @builtin(vertex_index) VertexIndex : u32,
              @builtin(instance_index) InstanceIndex : u32) -> Output {
              // TODO: remove workaround for Tint unary array access broke
              var zv : array<vec2<f32>, 4> = array<vec2<f32>, 4>(
                  vec2<f32>(0.2, 0.2),
                  vec2<f32>(0.3, 0.3),
                  vec2<f32>(-0.1, -0.1),
                  vec2<f32>(1.1, 1.1));
              let z : f32 = zv[InstanceIndex].x;

              var output : Output;
              output.Position = vec4<f32>(0.5, 0.5, z, 1.0);
              var colors : array<vec4<f32>, 4> = array<vec4<f32>, 4>(
                  vec4<f32>(1.0, 0.0, 0.0, 1.0),
                  vec4<f32>(0.0, 1.0, 0.0, 1.0),
                  vec4<f32>(0.0, 0.0, 1.0, 1.0),
                  vec4<f32>(1.0, 1.0, 1.0, 1.0)
              );
              output.color = colors[InstanceIndex];
              return output;
            }
            `,
        }),

        entryPoint: 'main',
      },

      fragment: {
        module: t.device.createShaderModule({
          code: `
            @stage(fragment) fn main(
              @location(0) color : vec4<f32>
              ) -> @location(0) vec4<f32> {
              return color;
            }
            `,
        }),

        entryPoint: 'main',
        targets: [{ format: colorAttachmentFormat }],
      },

      primitive: { topology: 'point-list' },
      depthStencil: {
        depthWriteEnabled: true,
        depthCompare: t.params.reversed ? 'greater' : 'less',
        format: depthBufferFormat,
      },
    };

    const pipeline = t.device.createRenderPipeline(pipelineDescriptor);

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: colorAttachmentView,
          storeOp: 'store',
          clearValue: { r: 0.5, g: 0.5, b: 0.5, a: 1.0 },
          loadOp: 'clear',
        },
      ],

      depthStencilAttachment: {
        view: depthTextureView,

        depthClearValue: t.params.reversed ? 0.0 : 1.0,
        depthLoadOp: 'clear',
        depthStoreOp: 'store',
      },
    });

    pass.setPipeline(pipeline);
    pass.draw(1, 4);
    pass.end();
    t.device.queue.submit([encoder.finish()]);

    t.expectSinglePixelIn2DTexture(
      colorAttachment,
      colorAttachmentFormat,
      { x: 0, y: 0 },
      {
        exp: new Uint8Array(
          t.params.reversed ? [0x00, 0xff, 0x00, 0xff] : [0xff, 0x00, 0x00, 0xff]
        ),
      }
    );
  });
