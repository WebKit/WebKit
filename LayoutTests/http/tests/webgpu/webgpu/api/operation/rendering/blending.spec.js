/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Test blending results.

TODO:
- Test result for all combinations of args (make sure each case is distinguishable from others
- Test underflow/overflow has consistent behavior
- ?
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert, unreachable } from '../../../../common/util/util.js';
import { kBlendFactors, kBlendOperations } from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';
import { float32ToFloat16Bits } from '../../../util/conversion.js';

export const g = makeTestGroup(GPUTest);

function mapColor(col, f) {
  return {
    r: f(col.r, 'r'),
    g: f(col.g, 'g'),
    b: f(col.b, 'b'),
    a: f(col.a, 'a'),
  };
}

function computeBlendFactor(src, dst, blendColor, factor) {
  switch (factor) {
    case 'zero':
      return { r: 0, g: 0, b: 0, a: 0 };
    case 'one':
      return { r: 1, g: 1, b: 1, a: 1 };
    case 'src':
      return { ...src };
    case 'one-minus-src':
      return mapColor(src, v => 1 - v);
    case 'src-alpha':
      return mapColor(src, () => src.a);
    case 'one-minus-src-alpha':
      return mapColor(src, () => 1 - src.a);
    case 'dst':
      return { ...dst };
    case 'one-minus-dst':
      return mapColor(dst, v => 1 - v);
    case 'dst-alpha':
      return mapColor(dst, () => dst.a);
    case 'one-minus-dst-alpha':
      return mapColor(dst, () => 1 - dst.a);
    case 'src-alpha-saturated': {
      const f = Math.min(src.a, 1 - dst.a);
      return { r: f, g: f, b: f, a: 1 };
    }
    case 'constant':
      assert(blendColor !== undefined);
      return { ...blendColor };
    case 'one-minus-constant':
      assert(blendColor !== undefined);
      return mapColor(blendColor, v => 1 - v);
    default:
      unreachable();
  }
}

function computeBlendOperation(src, srcFactor, dst, dstFactor, operation) {
  switch (operation) {
    case 'add':
      return mapColor(src, (_, k) => srcFactor[k] * src[k] + dstFactor[k] * dst[k]);
    case 'max':
      return mapColor(src, (_, k) => Math.max(src[k], dst[k]));
    case 'min':
      return mapColor(src, (_, k) => Math.min(src[k], dst[k]));
    case 'reverse-subtract':
      return mapColor(src, (_, k) => dstFactor[k] * dst[k] - srcFactor[k] * src[k]);
    case 'subtract':
      return mapColor(src, (_, k) => srcFactor[k] * src[k] - dstFactor[k] * dst[k]);
  }
}

g.test('GPUBlendComponent')
  .desc(
    `Test all combinations of parameters for GPUBlendComponent.

  Tests that parameters are correctly passed to the backend API and blend computations
  are done correctly by blending a single pixel. The test uses rgba16float as the format
  to avoid checking clamping behavior (tested in api,operation,rendering,blending:clamp,*).

  Params:
    - component= {color, alpha} - whether to test blending the color or the alpha component.
    - srcFactor= {...all GPUBlendFactors}
    - dstFactor= {...all GPUBlendFactors}
    - operation= {...all GPUBlendOperations}`
  )
  .params(u =>
    u //
      .combine('component', ['color', 'alpha'])
      .combine('srcFactor', kBlendFactors)
      .combine('dstFactor', kBlendFactors)
      .combine('operation', kBlendOperations)
      .filter(t => {
        if (t.operation === 'min' || t.operation === 'max') {
          return t.srcFactor === 'one' && t.dstFactor === 'one';
        }
        return true;
      })
      .beginSubcases()
      .combine('srcColor', [{ r: 0.11, g: 0.61, b: 0.81, a: 0.44 }])
      .combine('dstColor', [
        { r: 0.51, g: 0.22, b: 0.71, a: 0.33 },
        { r: 0.09, g: 0.73, b: 0.93, a: 0.81 },
      ])
      .expand('blendConstant', p => {
        const needsBlendConstant =
          p.srcFactor === 'one-minus-constant' ||
          p.srcFactor === 'constant' ||
          p.dstFactor === 'one-minus-constant' ||
          p.dstFactor === 'constant';
        return needsBlendConstant ? [{ r: 0.91, g: 0.82, b: 0.73, a: 0.64 }] : [undefined];
      })
  )
  .fn(t => {
    const textureFormat = 'rgba16float';
    const srcColor = t.params.srcColor;
    const dstColor = t.params.dstColor;
    const blendConstant = t.params.blendConstant;

    const srcFactor = computeBlendFactor(srcColor, dstColor, blendConstant, t.params.srcFactor);
    const dstFactor = computeBlendFactor(srcColor, dstColor, blendConstant, t.params.dstFactor);

    const expectedColor = computeBlendOperation(
      srcColor,
      srcFactor,
      dstColor,
      dstFactor,
      t.params.operation
    );

    switch (t.params.component) {
      case 'color':
        expectedColor.a = srcColor.a;
        break;
      case 'alpha':
        expectedColor.r = srcColor.r;
        expectedColor.g = srcColor.g;
        expectedColor.b = srcColor.b;
        break;
    }

    const pipeline = t.device.createRenderPipeline({
      fragment: {
        targets: [
          {
            format: textureFormat,
            blend: {
              // Set both color/alpha to defaults...
              color: {},
              alpha: {},
              // ... but then override the component we're testing.
              [t.params.component]: {
                srcFactor: t.params.srcFactor,
                dstFactor: t.params.dstFactor,
                operation: t.params.operation,
              },
            },
          },
        ],

        module: t.device.createShaderModule({
          code: `
struct Uniform {
  color: vec4<f32>
};
@group(0) @binding(0) var<uniform> u : Uniform;

@stage(fragment) fn main() -> @location(0) vec4<f32> {
  return u.color;
}
          `,
        }),

        entryPoint: 'main',
      },

      vertex: {
        module: t.device.createShaderModule({
          code: `
@stage(vertex) fn main() -> @builtin(position) vec4<f32> {
    return vec4<f32>(0.0, 0.0, 0.0, 1.0);
}
          `,
        }),

        entryPoint: 'main',
      },

      primitive: {
        topology: 'point-list',
      },
    });

    const renderTarget = t.device.createTexture({
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      size: [1, 1, 1],
      format: textureFormat,
    });

    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          view: renderTarget.createView(),
          clearValue: dstColor,
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    });

    renderPass.setPipeline(pipeline);
    if (blendConstant) {
      renderPass.setBlendConstant(blendConstant);
    }
    renderPass.setBindGroup(
      0,
      t.device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: {
              buffer: t.makeBufferWithContents(
                new Float32Array([srcColor.r, srcColor.g, srcColor.b, srcColor.a]),
                GPUBufferUsage.UNIFORM
              ),
            },
          },
        ],
      })
    );

    renderPass.draw(1);
    renderPass.end();

    t.device.queue.submit([commandEncoder.finish()]);

    const tolerance = 0.003;
    const expectedLow = mapColor(expectedColor, v => v - tolerance);
    const expectedHigh = mapColor(expectedColor, v => v + tolerance);

    t.expectSinglePixelBetweenTwoValuesFloat16In2DTexture(
      renderTarget,
      textureFormat,
      { x: 0, y: 0 },
      {
        exp: [
          // Use Uint16Array to store Float16 value bits
          new Uint16Array(
            [expectedLow.r, expectedLow.g, expectedLow.b, expectedLow.a].map(float32ToFloat16Bits)
          ),

          new Uint16Array(
            [expectedHigh.r, expectedHigh.g, expectedHigh.b, expectedHigh.a].map(
              float32ToFloat16Bits
            )
          ),
        ],
      }
    );
  });

g.test('formats')
  .desc(
    `Test blending results works for all formats that support it, and that blending is not applied
  for formats that do not. Blending should be done in linear space for srgb formats.`
  )
  .unimplemented();

g.test('clamp,blend_factor')
  .desc('For fixed-point formats, test that the blend factor is clamped in the blend equation.')
  .unimplemented();

g.test('clamp,blend_color')
  .desc('For fixed-point formats, test that the blend color is clamped in the blend equation.')
  .unimplemented();

g.test('clamp,blend_result')
  .desc('For fixed-point formats, test that the blend result is clamped in the blend equation.')
  .unimplemented();
