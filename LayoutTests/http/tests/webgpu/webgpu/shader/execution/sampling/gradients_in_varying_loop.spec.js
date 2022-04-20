/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests the behavior of gradient operations in a loop with potentially varying execution.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/util/util.js';
import { GPUTest } from '../../../gpu_test.js';

const kRTSize = 8; // Render target size (width and height)
const kBytesPerRow = 256; // Byte stride
const kColorAttachmentFormat = 'rgba32float';

const kDX = 0.1; // Desired partial derivative in x
const kDY = 0.2; // Desired partial derivative in y

// renders a two-triangle quad with uvs mapped a specific way so that dpdx/dpdy return expected values
class DerivativesTest extends GPUTest {
  copyRenderTargetToBuffer(rt) {
    const byteLength = kRTSize * kBytesPerRow;
    const buffer = this.device.createBuffer({
      size: byteLength,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const commandEncoder = this.device.createCommandEncoder();
    commandEncoder.copyTextureToBuffer(
      { texture: rt, mipLevel: 0, origin: [0, 0, 0] },
      { buffer, bytesPerRow: kBytesPerRow, rowsPerImage: kRTSize },
      { width: kRTSize, height: kRTSize, depthOrArrayLayers: 1 }
    );

    this.queue.submit([commandEncoder.finish()]);

    return buffer;
  }

  async init() {
    await super.init();

    this.pipeline = this.device.createRenderPipeline({
      vertex: {
        module: this.device.createShaderModule({
          code: `
            struct Outputs {
              @builtin(position) Position : vec4<f32>,
              @location(0) fragUV : vec2<f32>,
            };

            @stage(vertex) fn main(
              @builtin(vertex_index) VertexIndex : u32) -> Outputs {
              // Full screen quad
              var position : array<vec3<f32>, 6> = array<vec3<f32>, 6>(
                vec3<f32>(-1.0, 1.0, 0.0),
                vec3<f32>(1.0, 1.0,  0.0),
                vec3<f32>(1.0, -1.0, 0.0),
                vec3<f32>(1.0, -1.0, 0.0),
                vec3<f32>(-1.0, -1.0,  0.0),
                vec3<f32>(-1.0, 1.0,  0.0)
                );

              // Map UVs so that dpdx and dpdy return specific values for the first fragment quad
              let umax = ${kDX * kRTSize};
              let vmax = ${kDY * kRTSize};
              var uv : array<vec2<f32>, 6> = array<vec2<f32>, 6>(
                vec2<f32>(0.0, 0.0),
                vec2<f32>(umax, 0.0),
                vec2<f32>(umax, vmax),
                vec2<f32>(umax, vmax),
                vec2<f32>(0.0, vmax),
                vec2<f32>(0.0, 0.0));

              var output : Outputs;
              output.fragUV = uv[VertexIndex];
              output.Position = vec4<f32>(position[VertexIndex], 1.0);
              return output;
            }
            `,
        }),

        entryPoint: 'main',
      },

      fragment: {
        module: this.device.createShaderModule({
          code: `
            struct Uniforms {
              numIterations : i32
            };
            @binding(0) @group(0) var<uniform> uniforms : Uniforms;

            @stage(fragment) fn main(
              @builtin(position) FragCoord : vec4<f32>,
              @location(0) fragUV: vec2<f32>) -> @location(0) vec4<f32> {

                // Loop to exercise uniform control flow of gradient operations, to trip FXC's
                // warning X3570: gradient instruction used in a loop with varying iteration, attempting to unroll the loop
                var summed_dx : f32 = 0.0;
                var summed_dy : f32 = 0.0;
                for (var i = 0; i < uniforms.numIterations; i = i + 1) {

                  // Bogus condition to make this a "loop with varying iteration".
                  if (fragUV.x > 500.0) {
                    break;
                  }

                  // Do the gradient operations within the loop
                  let dx = dpdxCoarse(fragUV.x);
                  let dy = dpdyCoarse(fragUV.y);

                  summed_dx = summed_dx + dx;
                  summed_dy = summed_dy + dy;
                }

                return vec4<f32>(summed_dx, summed_dy, 0.0, 1.0);
            }
            `,
        }),

        entryPoint: 'main',
        targets: [{ format: kColorAttachmentFormat }],
      },

      primitive: { topology: 'triangle-list' },
    });
  }

  // return the render target texture object
  drawQuad(numIterations) {
    // make sure it's already initialized
    assert(this.pipeline !== undefined);

    const uniformBufferSize = 4; // numIterations : i32

    const uniformBuffer = this.device.createBuffer({
      size: uniformBufferSize,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    const uniforms = Int32Array.from([numIterations]);
    this.queue.writeBuffer(
      uniformBuffer,
      0,
      uniforms.buffer,
      uniforms.byteOffset,
      uniforms.byteLength
    );

    const bindGroup = this.device.createBindGroup({
      entries: [{ binding: 0, resource: { buffer: uniformBuffer } }],
      layout: this.pipeline.getBindGroupLayout(0),
    });

    const colorAttachment = this.device.createTexture({
      format: kColorAttachmentFormat,
      size: { width: kRTSize, height: kRTSize, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const colorAttachmentView = colorAttachment.createView();

    const encoder = this.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: colorAttachmentView,
          storeOp: 'store',
          clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
          loadOp: 'clear',
        },
      ],
    });

    pass.setPipeline(this.pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.draw(6);
    pass.end();
    this.device.queue.submit([encoder.finish()]);

    return colorAttachment;
  }
}

export const g = makeTestGroup(DerivativesTest);

g.test('derivative_in_varying_loop')
  .desc(
    `
    Derivative test that invokes dpdx/dpdy in a loop where the compiler cannot determine the
    number of iterations, nor uniform execution, at compile time. FXC typically unrolls such
    loops and fails with a timeout. See https://crbug.com/tint/1112.
    `
  )
  .params(u => u.combine('iterations', [1, 2, 3]))
  .fn(async t => {
    const byteLength = kRTSize * kBytesPerRow;

    const numIterations = t.params.iterations;
    const result = await t.readGPUBufferRangeTyped(
      t.copyRenderTargetToBuffer(t.drawQuad(numIterations)),
      { type: Float32Array, typedLength: byteLength / 4 }
    );

    const almostEqual = (a, b, epsilon = 0.000001) => Math.abs(a - b) <= epsilon;

    const expected_x = numIterations * kDX;
    const expected_y = numIterations * kDY;

    t.expect(
      almostEqual(result.data[0], expected_x),
      'Render results with numIterations * dx is ' + result.data[0] + ', expected: ' + expected_x
    );

    t.expect(
      almostEqual(result.data[1], expected_y),
      'Render results with numIterations * dy is ' + result.data[1] + ', expected: ' + expected_y
    );

    result.cleanup();
  });
