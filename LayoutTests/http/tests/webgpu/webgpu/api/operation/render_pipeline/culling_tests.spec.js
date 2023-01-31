/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `Test culling and rasterization state.

Test coverage:
Test all culling combinations of GPUFrontFace and GPUCullMode show the correct output.

Use 2 triangles with different winding orders:

- Test that the counter-clock wise triangle has correct output for:
  - All FrontFaces (ccw, cw)
  - All CullModes (none, front, back)
  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)
  - Some primitive topologies (triangle-list, TODO: triangle-strip)

- Test that the clock wise triangle has correct output for:
  - All FrontFaces (ccw, cw)
  - All CullModes (none, front, back)
  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)
  - Some primitive topologies (triangle-list, TODO: triangle-strip)
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { kTextureFormatInfo } from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';

function faceIsCulled(face, frontFace, cullMode) {
  return cullMode !== 'none' && (frontFace === face) === (cullMode === 'front');
}

function faceColor(face, frontFace, cullMode) {
  // front facing color is green, non front facing is red, background is blue
  const isCulled = faceIsCulled(face, frontFace, cullMode);
  if (!isCulled && face === frontFace) {
    return new Uint8Array([0x00, 0xff, 0x00, 0xff]);
  } else if (isCulled) {
    return new Uint8Array([0x00, 0x00, 0xff, 0xff]);
  } else {
    return new Uint8Array([0xff, 0x00, 0x00, 0xff]);
  }
}

export const g = makeTestGroup(GPUTest);

g.test('culling')
  .desc(
    `
TODO: test triangle-strip as well [1]
TODO: check the contents of the depth and stencil outputs [2]
`
  )
  .params(
    u =>
      u
        .combine('frontFace', ['ccw', 'cw'])
        .combine('cullMode', ['none', 'front', 'back'])
        .beginSubcases()
        .combine('depthStencilFormat', [
          null,
          'depth24plus',
          'depth32float',
          'depth24plus-stencil8',
        ])
        .combine('primitiveTopology', ['triangle-list']) // [1]
  )
  .fn(t => {
    const size = 4;
    const format = 'rgba8unorm';

    const texture = t.device.createTexture({
      size: { width: size, height: size, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
    });

    let depthTexture = undefined;
    let depthStencilAttachment = undefined;
    if (t.params.depthStencilFormat) {
      depthTexture = t.device.createTexture({
        size: { width: size, height: size, depthOrArrayLayers: 1 },
        format: t.params.depthStencilFormat,
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
      });

      depthStencilAttachment = {
        view: depthTexture.createView(),
        depthClearValue: 1.0,
        depthLoadOp: 'clear',
        depthStoreOp: 'store',
      };

      if (t.params.depthStencilFormat && kTextureFormatInfo[t.params.depthStencilFormat].stencil) {
        depthStencilAttachment.stencilClearValue = 0;
        depthStencilAttachment.stencilLoadOp = 'clear';
        depthStencilAttachment.stencilStoreOp = 'store';
      }
    }

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: texture.createView(),
          clearValue: { r: 0.0, g: 0.0, b: 1.0, a: 1.0 },
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],

      depthStencilAttachment,
    });

    // Draw two triangles with different winding orders:
    // 1. The top-left one is counterclockwise (CCW)
    // 2. The bottom-right one is clockwise (CW)
    pass.setPipeline(
      t.device.createRenderPipeline({
        layout: 'auto',
        vertex: {
          module: t.device.createShaderModule({
            code: `
              @vertex fn main(
                @builtin(vertex_index) VertexIndex : u32
                ) -> @builtin(position) vec4<f32> {
                var pos : array<vec2<f32>, 6> = array<vec2<f32>, 6>(
                    vec2<f32>(-1.0,  1.0),
                    vec2<f32>(-1.0,  0.0),
                    vec2<f32>( 0.0,  1.0),
                    vec2<f32>( 0.0, -1.0),
                    vec2<f32>( 1.0,  0.0),
                    vec2<f32>( 1.0, -1.0));
                return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
              }`,
          }),
          entryPoint: 'main',
        },
        fragment: {
          module: t.device.createShaderModule({
            code: `
              @fragment fn main(
                @builtin(front_facing) FrontFacing : bool
                ) -> @location(0) vec4<f32> {
                var color : vec4<f32>;
                if (FrontFacing) {
                  color = vec4<f32>(0.0, 1.0, 0.0, 1.0);
                } else {
                  color = vec4<f32>(1.0, 0.0, 0.0, 1.0);
                }
                return color;
              }`,
          }),
          entryPoint: 'main',
          targets: [{ format }],
        },
        primitive: {
          topology: t.params.primitiveTopology,
          frontFace: t.params.frontFace,
          cullMode: t.params.cullMode,
        },
        depthStencil: depthTexture ? { format: t.params.depthStencilFormat } : undefined,
      })
    );

    pass.draw(6, 1, 0, 0);
    pass.end();

    t.device.queue.submit([encoder.finish()]);

    // front facing color is green, non front facing is red, background is blue
    const kCCWTriangleTopLeftColor = faceColor('ccw', t.params.frontFace, t.params.cullMode);
    t.expectSinglePixelIn2DTexture(
      texture,
      format,
      { x: 0, y: 0 },
      { exp: kCCWTriangleTopLeftColor }
    );

    const kCWTriangleBottomRightColor = faceColor('cw', t.params.frontFace, t.params.cullMode);
    t.expectSinglePixelIn2DTexture(
      texture,
      format,
      { x: size - 1, y: size - 1 },
      { exp: kCWTriangleBottomRightColor }
    );

    // [2]: check the contents of the depth and stencil outputs
  });
