/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests that the final sample mask is the logical AND of all the relevant masks.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/util/util.js';
import { GPUTest } from '../../../gpu_test.js';
import { TypeF32, TypeU32 } from '../../../util/conversion.js';
import { makeTextureWithContents } from '../../../util/texture.js';
import { TexelView } from '../../../util/texture/texel_view.js';

const kColors = [
  // Red
  new Uint8Array([0xff, 0, 0, 0xff]),
  // Green
  new Uint8Array([0, 0xff, 0, 0xff]),
  // Blue
  new Uint8Array([0, 0, 0xff, 0xff]),
  // Yellow
  new Uint8Array([0xff, 0xff, 0, 0xff]),
];

const kDepthClearValue = 1.0;
const kDepthWriteValue = 0.0;
const kStencilClearValue = 0;
const kStencilReferenceValue = 0xff;

// Format of the render target and resolve target
const format = 'rgba8unorm';

// Format of depth stencil attachment
const depthStencilFormat = 'depth24plus-stencil8';

const kRenderTargetSize = 1;

function hasSample(rasterizationMask, sampleMask, fragmentShaderOutputMask, sampleIndex = 0) {
  return (rasterizationMask & sampleMask & fragmentShaderOutputMask & (1 << sampleIndex)) > 0;
}

class F extends GPUTest {
  GetTargetTexture(sampleCount, rasterizationMask, sampleMask, fragmentShaderOutputMask) {
    // Create a 2x2 color texture to sample from
    // texel 0 - Red
    // texel 1 - Green
    // texel 2 - Blue
    // texel 3 - Yellow
    const kSampleTextureSize = 2;
    const sampleTexture = makeTextureWithContents(
      this.device,
      TexelView.fromTexelsAsBytes(format, coord => {
        const id = coord.x + coord.y * kSampleTextureSize;
        return kColors[id];
      }),
      {
        size: [kSampleTextureSize, kSampleTextureSize, 1],
        usage:
          GPUTextureUsage.TEXTURE_BINDING |
          GPUTextureUsage.COPY_DST |
          GPUTextureUsage.RENDER_ATTACHMENT,
      }
    );

    const sampler = this.device.createSampler({
      magFilter: 'nearest',
      minFilter: 'nearest',
    });

    const fragmentMaskUniformBuffer = this.device.createBuffer({
      size: 4,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC,
    });
    this.trackForCleanup(fragmentMaskUniformBuffer);
    this.device.queue.writeBuffer(
      fragmentMaskUniformBuffer,
      0,
      new Uint32Array([fragmentShaderOutputMask])
    );

    const pipeline = this.device.createRenderPipeline({
      layout: 'auto',
      vertex: {
        module: this.device.createShaderModule({
          code: `
          struct VertexOutput {
            @builtin(position) Position : vec4<f32>,
            @location(0) @interpolate(perspective, sample) fragUV : vec2<f32>,
          }

          @vertex
          fn main(@builtin(vertex_index) VertexIndex : u32) -> VertexOutput {
            var pos = array<vec2<f32>, 30>(
                // center quad
                // only covers pixel center which is sample point when sampleCount === 1
                // small enough to avoid covering any multi sample points
                vec2<f32>( 0.2,  0.2),
                vec2<f32>( 0.2, -0.2),
                vec2<f32>(-0.2, -0.2),
                vec2<f32>( 0.2,  0.2),
                vec2<f32>(-0.2, -0.2),
                vec2<f32>(-0.2,  0.2),

                // Sub quads are representing rasterization mask and
                // are slightly scaled to avoid covering the pixel center

                // top-left quad
                vec2<f32>( -0.01, 1.0),
                vec2<f32>( -0.01, 0.01),
                vec2<f32>(-1.0, 0.01),
                vec2<f32>( -0.01, 1.0),
                vec2<f32>(-1.0, 0.01),
                vec2<f32>(-1.0, 1.0),

                // top-right quad
                vec2<f32>(1.0, 1.0),
                vec2<f32>(1.0, 0.01),
                vec2<f32>(0.01, 0.01),
                vec2<f32>(1.0, 1.0),
                vec2<f32>(0.01, 0.01),
                vec2<f32>(0.01, 1.0),

                // bottom-left quad
                vec2<f32>( -0.01,  -0.01),
                vec2<f32>( -0.01, -1.0),
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( -0.01,  -0.01),
                vec2<f32>(-1.0, -1.0),
                vec2<f32>(-1.0,  -0.01),

                // bottom-right quad
                vec2<f32>(1.0,  -0.01),
                vec2<f32>(1.0, -1.0),
                vec2<f32>(0.01, -1.0),
                vec2<f32>(1.0,  -0.01),
                vec2<f32>(0.01, -1.0),
                vec2<f32>(0.01,  -0.01)
              );

            var uv = array<vec2<f32>, 30>(
                // center quad
                vec2<f32>(1.0, 0.0),
                vec2<f32>(1.0, 1.0),
                vec2<f32>(0.0, 1.0),
                vec2<f32>(1.0, 0.0),
                vec2<f32>(0.0, 1.0),
                vec2<f32>(0.0, 0.0),

                // top-left quad (texel 0)
                vec2<f32>(0.5, 0.0),
                vec2<f32>(0.5, 0.5),
                vec2<f32>(0.0, 0.5),
                vec2<f32>(0.5, 0.0),
                vec2<f32>(0.0, 0.5),
                vec2<f32>(0.0, 0.0),

                // top-right quad (texel 1)
                vec2<f32>(1.0, 0.0),
                vec2<f32>(1.0, 0.5),
                vec2<f32>(0.5, 0.5),
                vec2<f32>(1.0, 0.0),
                vec2<f32>(0.5, 0.5),
                vec2<f32>(0.5, 0.0),

                // bottom-left quad (texel 2)
                vec2<f32>(0.5, 0.5),
                vec2<f32>(0.5, 1.0),
                vec2<f32>(0.0, 1.0),
                vec2<f32>(0.5, 0.5),
                vec2<f32>(0.0, 1.0),
                vec2<f32>(0.0, 0.5),

                // bottom-right quad (texel 3)
                vec2<f32>(1.0, 0.5),
                vec2<f32>(1.0, 1.0),
                vec2<f32>(0.5, 1.0),
                vec2<f32>(1.0, 0.5),
                vec2<f32>(0.5, 1.0),
                vec2<f32>(0.5, 0.5)
              );

            var output : VertexOutput;
            output.Position = vec4<f32>(pos[VertexIndex], ${kDepthWriteValue}, 1.0);
            output.fragUV = uv[VertexIndex];
            return output;
          }`,
        }),
        entryPoint: 'main',
      },
      fragment: {
        module: this.device.createShaderModule({
          code: `
          @group(0) @binding(0) var mySampler: sampler;
          @group(0) @binding(1) var myTexture: texture_2d<f32>;
          @group(0) @binding(2) var<uniform> fragMask: u32;

          struct FragmentOutput {
            @builtin(sample_mask) mask : u32,
            @location(0) color : vec4<f32>,
          }

          @fragment
          fn main(@location(0) @interpolate(perspective, sample) fragUV: vec2<f32>) -> FragmentOutput {
            return FragmentOutput(fragMask, textureSample(myTexture, mySampler, fragUV));
          }`,
        }),
        entryPoint: 'main',
        targets: [{ format }],
      },
      primitive: { topology: 'triangle-list' },
      multisample: {
        count: sampleCount,
        mask: sampleMask,
        alphaToCoverageEnabled: false,
      },
      depthStencil: {
        format: depthStencilFormat,
        depthWriteEnabled: true,
        depthCompare: 'always',

        stencilFront: {
          compare: 'always',
          passOp: 'replace',
        },
        stencilBack: {
          compare: 'always',
          passOp: 'replace',
        },
      },
    });

    const uniformBindGroup = this.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: sampler,
        },
        {
          binding: 1,
          resource: sampleTexture.createView(),
        },
        {
          binding: 2,
          resource: {
            buffer: fragmentMaskUniformBuffer,
          },
        },
      ],
    });

    const renderTargetTexture = this.device.createTexture({
      format,
      size: {
        width: kRenderTargetSize,
        height: kRenderTargetSize,
        depthOrArrayLayers: 1,
      },
      sampleCount,
      mipLevelCount: 1,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
    });
    const resolveTargetTexture =
      sampleCount === 1
        ? null
        : this.device.createTexture({
            format,
            size: {
              width: kRenderTargetSize,
              height: kRenderTargetSize,
              depthOrArrayLayers: 1,
            },
            sampleCount: 1,
            mipLevelCount: 1,
            usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
          });

    const depthStencilTexture = this.device.createTexture({
      size: {
        width: kRenderTargetSize,
        height: kRenderTargetSize,
      },
      format: depthStencilFormat,
      sampleCount,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
    });

    const renderPassDescriptor = {
      colorAttachments: [
        {
          view: renderTargetTexture.createView(),
          resolveTarget: resolveTargetTexture?.createView(),

          clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],

      depthStencilAttachment: {
        view: depthStencilTexture.createView(),
        depthClearValue: kDepthClearValue,
        depthLoadOp: 'clear',
        depthStoreOp: 'store',
        stencilClearValue: kStencilClearValue,
        stencilLoadOp: 'clear',
        stencilStoreOp: 'store',
      },
    };
    const commandEncoder = this.device.createCommandEncoder();
    const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setBindGroup(0, uniformBindGroup);
    passEncoder.setStencilReference(kStencilReferenceValue);

    if (sampleCount === 1) {
      if ((rasterizationMask & 1) !== 0) {
        // draw center quad
        passEncoder.draw(6);
      }
    } else {
      assert(sampleCount === 4);
      if ((rasterizationMask & 1) !== 0) {
        // draw top-left quad
        passEncoder.draw(6, 1, 6);
      }
      if ((rasterizationMask & 2) !== 0) {
        // draw top-right quad
        passEncoder.draw(6, 1, 12);
      }
      if ((rasterizationMask & 4) !== 0) {
        // draw bottom-left quad
        passEncoder.draw(6, 1, 18);
      }
      if ((rasterizationMask & 8) !== 0) {
        // draw bottom-right quad
        passEncoder.draw(6, 1, 24);
      }
    }
    passEncoder.end();
    this.device.queue.submit([commandEncoder.finish()]);

    return {
      color: renderTargetTexture,
      depthStencil: depthStencilTexture,
    };
  }

  CheckColorAttachmentResult(
    texture,
    sampleCount,
    rasterizationMask,
    sampleMask,
    fragmentShaderOutputMask
  ) {
    const buffer = this.copySinglePixelTextureToBufferUsingComputePass(
      TypeF32, // correspond to 'rgba8unorm' format
      4,
      texture.createView(),
      sampleCount
    );

    const expectedDstData = new Float32Array(sampleCount * 4);
    if (sampleCount === 1) {
      if (hasSample(rasterizationMask, sampleMask, fragmentShaderOutputMask)) {
        // Texel 3 is sampled at the pixel center
        expectedDstData[0] = kColors[3][0] / 0xff;
        expectedDstData[1] = kColors[3][1] / 0xff;
        expectedDstData[2] = kColors[3][2] / 0xff;
        expectedDstData[3] = kColors[3][3] / 0xff;
      }
    } else {
      for (let i = 0; i < sampleCount; i++) {
        if (hasSample(rasterizationMask, sampleMask, fragmentShaderOutputMask, i)) {
          const o = i * 4;
          expectedDstData[o + 0] = kColors[i][0] / 0xff;
          expectedDstData[o + 1] = kColors[i][1] / 0xff;
          expectedDstData[o + 2] = kColors[i][2] / 0xff;
          expectedDstData[o + 3] = kColors[i][3] / 0xff;
        }
      }
    }

    this.expectGPUBufferValuesEqual(buffer, expectedDstData);
  }

  CheckDepthStencilResult(
    aspect,
    depthStencilTexture,
    sampleCount,
    rasterizationMask,
    sampleMask,
    fragmentShaderOutputMask
  ) {
    const buffer = this.copySinglePixelTextureToBufferUsingComputePass(
      // Use f32 as the scalar type for depth (depth24plus, depth32float)
      // Use u32 as the scalar type for stencil (stencil8)
      aspect === 'depth-only' ? TypeF32 : TypeU32,
      1,
      depthStencilTexture.createView({ aspect }),
      sampleCount
    );

    const expectedDstData =
      aspect === 'depth-only' ? new Float32Array(sampleCount) : new Uint32Array(sampleCount);
    for (let i = 0; i < sampleCount; i++) {
      const s = hasSample(rasterizationMask, sampleMask, fragmentShaderOutputMask, i);
      if (aspect === 'depth-only') {
        expectedDstData[i] = s ? kDepthWriteValue : kDepthClearValue;
      } else {
        expectedDstData[i] = s ? kStencilReferenceValue : kStencilClearValue;
      }
    }
    this.expectGPUBufferValuesEqual(buffer, expectedDstData);
  }
}

export const g = makeTestGroup(F);

g.test('final_output')
  .desc(
    `
Tests that the final sample mask is the logical AND of all the relevant masks -- meaning that the samples
not included in the final mask are discarded on any attachments including
- color outputs
- depth tests
- stencil operations

The test draws 0/1/1+ textured quads of which each sample in the standard 4-sample pattern results in a different color:
- Sample 0, Texel 0, top-left: Red
- Sample 1, Texel 1, top-left: Green
- Sample 2, Texel 2, top-left: Blue
- Sample 3, Texel 3, top-left: Yellow

The test checks each sample value of the render target texture and depth stencil texture using a compute pass to
textureLoad each sample index from the texture and write to a storage buffer to compare with expected values.

- for sampleCount = { 1, 4 } and various combinations of:
    - rasterization mask = { 0, ..., 2 ** sampleCount - 1 }
    - sample mask = { 0, 0b0001, 0b0010, 0b0111, 0b1011, 0b1101, 0b1110, 0b1111, 0b11110 }
    - fragment shader output @builtin(sample_mask) = { 0, 0b0001, 0b0010, 0b0111, 0b1011, 0b1101, 0b1110, 0b1111, 0b11110 }
- [choosing 0b11110 because the 5th bit should be ignored]
`
  )
  .params(u =>
    u
      .combine('sampleCount', [1, 4])
      .expand('rasterizationMask', function* (p) {
        for (let i = 0, len = 2 ** p.sampleCount - 1; i <= len; i++) {
          yield i;
        }
      })
      .beginSubcases()
      .combine('sampleMask', [0, 0b0001, 0b0010, 0b0111, 0b1011, 0b1101, 0b1110, 0b1111, 0b11110])
      .combine('fragmentShaderOutputMask', [
        0,
        0b0001,
        0b0010,
        0b0111,
        0b1011,
        0b1101,
        0b1110,
        0b1111,
        0b11110,
      ])
  )
  .fn(t => {
    const { sampleCount, rasterizationMask, sampleMask, fragmentShaderOutputMask } = t.params;

    const { color, depthStencil } = t.GetTargetTexture(
      sampleCount,
      rasterizationMask,
      sampleMask,
      fragmentShaderOutputMask
    );

    t.CheckColorAttachmentResult(
      color,
      sampleCount,
      rasterizationMask,
      sampleMask,
      fragmentShaderOutputMask
    );

    t.CheckDepthStencilResult(
      'depth-only',
      depthStencil,
      sampleCount,
      rasterizationMask,
      sampleMask,
      fragmentShaderOutputMask
    );

    t.CheckDepthStencilResult(
      'stencil-only',
      depthStencil,
      sampleCount,
      rasterizationMask,
      sampleMask,
      fragmentShaderOutputMask
    );
  });
