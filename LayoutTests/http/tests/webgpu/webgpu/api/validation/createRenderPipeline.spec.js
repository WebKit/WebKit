/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
createRenderPipeline and createRenderPipelineAsync validation tests.

TODO: review existing tests, write descriptions, and make sure tests are complete.
      Make sure the following is covered. Consider splitting the file if too large/disjointed.
> - various attachment problems
>
> - interface matching between vertex and fragment shader
>     - superset, subset, etc.
>
> - vertex stage {valid, invalid}
> - fragment stage {valid, invalid}
> - primitive topology all possible values
> - rasterizationState various values
> - multisample count {0, 1, 3, 4, 8, 16, 1024}
> - multisample mask {0, 0xFFFFFFFF}
> - alphaToCoverage:
>     - alphaToCoverageEnabled is { true, false } and sampleCount { = 1, = 4 }.
>       The only failing case is (true, 1).
>     - output SV_Coverage semantics is statically used by fragmentStage and
>       alphaToCoverageEnabled is { true (fails), false (passes) }.
>     - sampleMask is being used and alphaToCoverageEnabled is { true (fails), false (passes) }.

`;
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { unreachable } from '../../../common/util/util.js';
import {
  kTextureFormats,
  kRenderableColorTextureFormats,
  kTextureFormatInfo,
  kDepthStencilFormats,
  kCompareFunctions,
  kStencilOperations,
  kBlendFactors,
  kBlendOperations,
} from '../../capability_info.js';
import { kTexelRepresentationInfo } from '../../util/texture/texel_data.js';

import { ValidationTest } from './validation_test.js';

class F extends ValidationTest {
  getFragmentShaderCode(sampleType, componentCount) {
    const v = ['0', '1', '0', '1'];

    let fragColorType;
    let suffix;
    switch (sampleType) {
      case 'sint':
        fragColorType = 'i32';
        suffix = '';
        break;
      case 'uint':
        fragColorType = 'u32';
        suffix = 'u';
        break;
      default:
        fragColorType = 'f32';
        suffix = '.0';
        break;
    }

    let outputType;
    let result;
    switch (componentCount) {
      case 1:
        outputType = fragColorType;
        result = `${v[0]}${suffix}`;
        break;
      case 2:
        outputType = `vec2<${fragColorType}>`;
        result = `${outputType}(${v[0]}${suffix}, ${v[1]}${suffix})`;
        break;
      case 3:
        outputType = `vec3<${fragColorType}>`;
        result = `${outputType}(${v[0]}${suffix}, ${v[1]}${suffix}, ${v[2]}${suffix})`;
        break;
      case 4:
        outputType = `vec4<${fragColorType}>`;
        result = `${outputType}(${v[0]}${suffix}, ${v[1]}${suffix}, ${v[2]}${suffix}, ${v[3]}${suffix})`;
        break;
      default:
        unreachable();
    }

    return `
    @stage(fragment) fn main() -> @location(0) ${outputType} {
      return ${result};
    }`;
  }

  getDescriptor(options = {}) {
    const defaultTargets = [{ format: 'rgba8unorm' }];
    const {
      topology = 'triangle-list',
      targets = defaultTargets,
      sampleCount = 1,
      depthStencil,
      fragmentShaderCode = this.getFragmentShaderCode(
        kTextureFormatInfo[targets[0] ? targets[0].format : 'rgba8unorm'].sampleType,
        4
      ),

      noFragment = false,
    } = options;

    return {
      vertex: {
        module: this.device.createShaderModule({
          code: `
            @stage(vertex) fn main() -> @builtin(position) vec4<f32> {
              return vec4<f32>(0.0, 0.0, 0.0, 1.0);
            }`,
        }),

        entryPoint: 'main',
      },

      fragment: noFragment
        ? undefined
        : {
            module: this.device.createShaderModule({
              code: fragmentShaderCode,
            }),

            entryPoint: 'main',
            targets,
          },

      layout: this.getPipelineLayout(),
      primitive: { topology },
      multisample: { count: sampleCount },
      depthStencil,
    };
  }

  getPipelineLayout() {
    return this.device.createPipelineLayout({ bindGroupLayouts: [] });
  }

  createTexture(params) {
    const { format, sampleCount } = params;

    return this.device.createTexture({
      size: { width: 4, height: 4, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
      format,
      sampleCount,
    });
  }

  doCreateRenderPipelineTest(isAsync, _success, descriptor) {
    if (isAsync) {
      if (_success) {
        this.shouldResolve(this.device.createRenderPipelineAsync(descriptor));
      } else {
        this.shouldReject('OperationError', this.device.createRenderPipelineAsync(descriptor));
      }
    } else {
      this.expectValidationError(() => {
        this.device.createRenderPipeline(descriptor);
      }, !_success);
    }
  }
}

export const g = makeTestGroup(F);

g.test('basic_use_of_createRenderPipeline')
  .desc(`TODO: review and add description; shorten name`)
  .params(u => u.combine('isAsync', [false, true]))
  .fn(async t => {
    const { isAsync } = t.params;
    const descriptor = t.getDescriptor();

    t.doCreateRenderPipelineTest(isAsync, true, descriptor);
  });

g.test('create_vertex_only_pipeline_with_without_depth_stencil_state')
  .desc(
    `Test creating vertex-only render pipeline. A vertex-only render pipeline have no fragment
state (and thus have no color state), and can be create with or without depth stencil state.

TODO: review and shorten name`
  )
  .params(u =>
    u
      .combine('isAsync', [false, true])
      .beginSubcases()
      .combine('depthStencilFormat', ['depth24plus', 'depth24plus-stencil8', 'depth32float', ''])
      .combine('haveColor', [false, true])
  )
  .fn(async t => {
    const { isAsync, depthStencilFormat, haveColor } = t.params;

    let depthStencilState;
    if (depthStencilFormat === '') {
      depthStencilState = undefined;
    } else {
      depthStencilState = { format: depthStencilFormat };
    }

    // Having targets or not should have no effect in result, since it will not appear in the
    // descriptor in vertex-only render pipeline
    const descriptor = t.getDescriptor({
      noFragment: true,
      depthStencil: depthStencilState,
      targets: haveColor ? [{ format: 'rgba8unorm' }] : [],
    });

    t.doCreateRenderPipelineTest(isAsync, true, descriptor);
  });

g.test('at_least_one_color_state_is_required_for_complete_pipeline')
  .desc(`TODO: review and add description; shorten name`)
  .params(u => u.combine('isAsync', [false, true]))
  .fn(async t => {
    const { isAsync } = t.params;

    const goodDescriptor = t.getDescriptor({
      targets: [{ format: 'rgba8unorm' }],
    });

    // Control case
    t.doCreateRenderPipelineTest(isAsync, true, goodDescriptor);

    // Fail because lack of color states
    const badDescriptor = t.getDescriptor({
      targets: [],
    });

    t.doCreateRenderPipelineTest(isAsync, false, badDescriptor);
  });

g.test('color_formats_must_be_renderable')
  .desc(`TODO: review and add description; shorten name`)
  .params(u => u.combine('isAsync', [false, true]).combine('format', kTextureFormats))
  .fn(async t => {
    const { isAsync, format } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    const descriptor = t.getDescriptor({ targets: [{ format }] });

    t.doCreateRenderPipelineTest(isAsync, info.renderable && info.color, descriptor);
  });

g.test('depth_stencil_state,format')
  .desc(`The texture format in depthStencilState must be a depth/stencil format`)
  .params(u => u.combine('isAsync', [false, true]).combine('format', kTextureFormats))
  .fn(async t => {
    const { isAsync, format } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    const descriptor = t.getDescriptor({ depthStencil: { format } });

    t.doCreateRenderPipelineTest(isAsync, info.depth || info.stencil, descriptor);
  });

g.test('depth_stencil_state,depth_aspect,depth_test')
  .desc(
    `Depth aspect must be contained in the format if depth test is enabled in depthStencilState.`
  )
  .params(u =>
    u
      .combine('isAsync', [false, true])
      .combine('format', kDepthStencilFormats)
      .combine('depthCompare', [undefined, ...kCompareFunctions])
  )
  .fn(async t => {
    const { isAsync, format, depthCompare } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    const descriptor = t.getDescriptor({
      depthStencil: { format, depthCompare },
    });

    const depthTestEnabled = depthCompare !== undefined && depthCompare !== 'always';
    t.doCreateRenderPipelineTest(isAsync, !depthTestEnabled || info.depth, descriptor);
  });

g.test('depth_stencil_state,depth_aspect,depth_write')
  .desc(
    `Depth aspect must be contained in the format if depth write is enabled in depthStencilState.`
  )
  .params(u =>
    u
      .combine('isAsync', [false, true])
      .combine('format', kDepthStencilFormats)
      .combine('depthWriteEnabled', [false, true])
  )
  .fn(async t => {
    const { isAsync, format, depthWriteEnabled } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    const descriptor = t.getDescriptor({
      depthStencil: { format, depthWriteEnabled },
    });

    t.doCreateRenderPipelineTest(isAsync, !depthWriteEnabled || info.depth, descriptor);
  });

g.test('depth_stencil_state,stencil_aspect,stencil_test')
  .desc(
    `Stencil aspect must be contained in the format if stencil test is enabled in depthStencilState.`
  )
  .params(u =>
    u
      .combine('isAsync', [false, true])
      .combine('format', kDepthStencilFormats)
      .combine('face', ['front', 'back'])
      .combine('compare', [undefined, ...kCompareFunctions])
  )
  .fn(async t => {
    const { isAsync, format, face, compare } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    let descriptor;
    if (face === 'front') {
      descriptor = t.getDescriptor({ depthStencil: { format, stencilFront: { compare } } });
    } else {
      descriptor = t.getDescriptor({ depthStencil: { format, stencilBack: { compare } } });
    }

    const stencilTestEnabled = compare !== undefined && compare !== 'always';
    t.doCreateRenderPipelineTest(isAsync, !stencilTestEnabled || info.stencil, descriptor);
  });

g.test('depth_stencil_state,stencil_aspect,stencil_write')
  .desc(
    `Stencil aspect must be contained in the format if stencil write is enabled in depthStencilState.`
  )
  .params(u =>
    u
      .combine('isAsync', [false, true])
      .combine('format', kDepthStencilFormats)
      .combine('faceAndOpType', [
        'frontFailOp',
        'frontDepthFailOp',
        'frontPassOp',
        'backFailOp',
        'backDepthFailOp',
        'backPassOp',
      ])
      .combine('op', [undefined, ...kStencilOperations])
  )
  .fn(async t => {
    const { isAsync, format, faceAndOpType, op } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    let depthStencil;
    switch (faceAndOpType) {
      case 'frontFailOp':
        depthStencil = { format, stencilFront: { failOp: op } };
        break;
      case 'frontDepthFailOp':
        depthStencil = { format, stencilFront: { depthFailOp: op } };
        break;
      case 'frontPassOp':
        depthStencil = { format, stencilFront: { passOp: op } };
        break;
      case 'backFailOp':
        depthStencil = { format, stencilBack: { failOp: op } };
        break;
      case 'backDepthFailOp':
        depthStencil = { format, stencilBack: { depthFailOp: op } };
        break;
      case 'backPassOp':
        depthStencil = { format, stencilBack: { passOp: op } };
        break;
      default:
        unreachable();
    }

    const descriptor = t.getDescriptor({ depthStencil });

    const stencilWriteEnabled = op !== undefined && op !== 'keep';
    t.doCreateRenderPipelineTest(isAsync, !stencilWriteEnabled || info.stencil, descriptor);
  });

g.test('sample_count_must_be_valid')
  .desc(`TODO: review and add description; shorten name`)
  .params(u =>
    u.combine('isAsync', [false, true]).combineWithParams([
      { sampleCount: 0, _success: false },
      { sampleCount: 1, _success: true },
      { sampleCount: 2, _success: false },
      { sampleCount: 3, _success: false },
      { sampleCount: 4, _success: true },
      { sampleCount: 8, _success: false },
      { sampleCount: 16, _success: false },
    ])
  )
  .fn(async t => {
    const { isAsync, sampleCount, _success } = t.params;

    const descriptor = t.getDescriptor({ sampleCount });

    t.doCreateRenderPipelineTest(isAsync, _success, descriptor);
  });

g.test('pipeline_output_targets')
  .desc(
    `Pipeline fragment output types must be compatible with target color state format
  - The scalar type (f32, i32, or u32) must match the sample type of the format.
  - The componentCount of the fragment output (e.g. f32, vec2, vec3, vec4) must not have fewer
    channels than that of the color attachment texture formats. Extra components are allowed and are discarded.

  MAINTENANCE_TODO: update this test after the WebGPU SPEC ISSUE 50 "define what 'compatible' means
  for render target formats" is resolved.`
  )
  .params(u =>
    u
      .combine('isAsync', [false, true])
      .combine('format', kRenderableColorTextureFormats)
      .beginSubcases()
      .combine('sampleType', ['float', 'uint', 'sint'])
      .combine('componentCount', [1, 2, 3, 4])
  )
  .fn(async t => {
    const { isAsync, format, sampleType, componentCount } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    const descriptor = t.getDescriptor({
      targets: [{ format }],
      fragmentShaderCode: t.getFragmentShaderCode(sampleType, componentCount),
    });

    const sampleTypeSuccess =
      info.sampleType === 'float' || info.sampleType === 'unfilterable-float'
        ? sampleType === 'float'
        : info.sampleType === sampleType;

    const _success =
      sampleTypeSuccess && componentCount >= kTexelRepresentationInfo[format].componentOrder.length;
    t.doCreateRenderPipelineTest(isAsync, _success, descriptor);
  });

g.test('pipeline_output_targets,blend')
  .desc(
    `On top of requirements from pipeline_output_targets, when blending is enabled and alpha channel is read indicated by any blend factor, an extra requirement is added:
  - fragment output must be vec4.
  `
  )
  .params(u =>
    u
      .combine('isAsync', [false, true])
      .combine('format', ['r8unorm', 'rg8unorm', 'rgba8unorm', 'bgra8unorm'])
      .beginSubcases()
      .combine('componentCount', [1, 2, 3, 4])
      .combineWithParams([
        // extra requirement does not apply
        {
          colorSrcFactor: 'one',
          colorDstFactor: 'zero',
          alphaSrcFactor: 'zero',
          alphaDstFactor: 'zero',
        },

        {
          colorSrcFactor: 'dst-alpha',
          colorDstFactor: 'zero',
          alphaSrcFactor: 'zero',
          alphaDstFactor: 'zero',
        },

        // extra requirement applies, fragment output must be vec4 (contain alpha channel)
        {
          colorSrcFactor: 'src-alpha',
          colorDstFactor: 'one',
          alphaSrcFactor: 'zero',
          alphaDstFactor: 'zero',
        },

        {
          colorSrcFactor: 'one',
          colorDstFactor: 'one-minus-src-alpha',
          alphaSrcFactor: 'zero',
          alphaDstFactor: 'zero',
        },

        {
          colorSrcFactor: 'src-alpha-saturated',
          colorDstFactor: 'one',
          alphaSrcFactor: 'zero',
          alphaDstFactor: 'zero',
        },

        {
          colorSrcFactor: 'one',
          colorDstFactor: 'zero',
          alphaSrcFactor: 'one',
          alphaDstFactor: 'zero',
        },

        {
          colorSrcFactor: 'one',
          colorDstFactor: 'zero',
          alphaSrcFactor: 'zero',
          alphaDstFactor: 'src',
        },

        {
          colorSrcFactor: 'one',
          colorDstFactor: 'zero',
          alphaSrcFactor: 'zero',
          alphaDstFactor: 'src-alpha',
        },
      ])
  )
  .fn(async t => {
    const sampleType = 'float';
    const {
      isAsync,
      format,
      componentCount,
      colorSrcFactor,
      colorDstFactor,
      alphaSrcFactor,
      alphaDstFactor,
    } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    const descriptor = t.getDescriptor({
      targets: [
        {
          format,
          blend: {
            color: {
              srcFactor: colorSrcFactor,
              dstFactor: colorDstFactor,
              operation: 'add',
            },

            alpha: {
              srcFactor: alphaSrcFactor,
              dstFactor: alphaDstFactor,
              operation: 'add',
            },
          },
        },
      ],

      fragmentShaderCode: t.getFragmentShaderCode(sampleType, componentCount),
    });

    const colorBlendReadsSrcAlpha =
      colorSrcFactor.includes('src-alpha') || colorDstFactor.includes('src-alpha');
    const meetsExtraBlendingRequirement = !colorBlendReadsSrcAlpha || componentCount === 4;
    const _success =
      info.sampleType === sampleType &&
      componentCount >= kTexelRepresentationInfo[format].componentOrder.length &&
      meetsExtraBlendingRequirement;
    t.doCreateRenderPipelineTest(isAsync, _success, descriptor);
  });

g.test('pipeline_output_targets,format_blendable')
  .desc(
    `
Tests if blending is used, the target's format must be blendable (support "float" sample type).
- For all the formats, test that blending can be enabled if and only if the format is blendable.`
  )
  .params(u =>
    u.combine('isAsync', [false, true]).combine('format', kRenderableColorTextureFormats)
  )
  .fn(async t => {
    const { isAsync, format } = t.params;
    const info = kTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.feature);

    const _success = info.sampleType === 'float';

    const blendComponent = {
      srcFactor: 'src-alpha',
      dstFactor: 'dst-alpha',
      operation: 'add',
    };

    t.doCreateRenderPipelineTest(
      isAsync,
      _success,
      t.getDescriptor({
        targets: [
          {
            format,
            blend: {
              color: blendComponent,
              alpha: blendComponent,
            },
          },
        ],

        fragmentShaderCode: t.getFragmentShaderCode('float', 4),
      })
    );
  });

g.test('pipeline_output_targets,blend_min_max')
  .desc(
    `
  For the blend components on either GPUBlendState.color or GPUBlendState.alpha:
  - Tests if the combination of 'srcFactor', 'dstFactor' and 'operation' is valid (if the blend
    operation is "min" or "max", srcFactor and dstFactor must be "one").
  `
  )
  .params(u =>
    u
      .combine('isAsync', [false, true])
      .combine('component', ['color', 'alpha'])
      .beginSubcases()
      .combine('srcFactor', kBlendFactors)
      .combine('dstFactor', kBlendFactors)
      .combine('operation', kBlendOperations)
  )
  .fn(async t => {
    const { isAsync, component, srcFactor, dstFactor, operation } = t.params;

    const defaultBlendComponent = {
      srcFactor: 'src-alpha',
      dstFactor: 'dst-alpha',
      operation: 'add',
    };

    const blendComponentToTest = {
      srcFactor,
      dstFactor,
      operation,
    };

    const fragmentShaderCode = t.getFragmentShaderCode('float', 4);
    const format = 'rgba8unorm';

    const descriptor = t.getDescriptor({
      targets: [
        {
          format,
          blend: {
            color: component === 'color' ? blendComponentToTest : defaultBlendComponent,
            alpha: component === 'alpha' ? blendComponentToTest : defaultBlendComponent,
          },
        },
      ],

      fragmentShaderCode,
    });

    if (operation === 'min' || operation === 'max') {
      const _success = srcFactor === 'one' && dstFactor === 'one';
      t.doCreateRenderPipelineTest(isAsync, _success, descriptor);
    } else {
      t.doCreateRenderPipelineTest(isAsync, true, descriptor);
    }
  });

g.test('pipeline_layout,device_mismatch')
  .desc(
    'Tests createRenderPipeline(Async) cannot be called with a pipeline layout created from another device'
  )
  .paramsSubcasesOnly(u => u.combine('isAsync', [true, false]).combine('mismatched', [true, false]))
  .fn(async t => {
    const { isAsync, mismatched } = t.params;

    if (mismatched) {
      await t.selectMismatchedDeviceOrSkipTestCase(undefined);
    }

    const device = mismatched ? t.mismatchedDevice : t.device;

    const layout = device.createPipelineLayout({ bindGroupLayouts: [] });

    const descriptor = {
      layout,
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

      fragment: {
        module: t.device.createShaderModule({ code: t.getFragmentShaderCode('float', 4) }),
        entryPoint: 'main',
        targets: [{ format: 'rgba8unorm' }],
      },
    };

    t.doCreateRenderPipelineTest(isAsync, !mismatched, descriptor);
  });

g.test('shader_module,device_mismatch')
  .desc(
    'Tests createRenderPipeline(Async) cannot be called with a shader module created from another device'
  )
  .paramsSubcasesOnly(u =>
    u.combine('isAsync', [true, false]).combineWithParams([
      { vertex_mismatched: false, fragment_mismatched: false, _success: true },
      { vertex_mismatched: true, fragment_mismatched: false, _success: false },
      { vertex_mismatched: false, fragment_mismatched: true, _success: false },
    ])
  )
  .fn(async t => {
    const { isAsync, vertex_mismatched, fragment_mismatched, _success } = t.params;

    if (vertex_mismatched || fragment_mismatched) {
      await t.selectMismatchedDeviceOrSkipTestCase(undefined);
    }

    const code = `
      @stage(vertex) fn main() -> @builtin(position) vec4<f32> {
        return vec4<f32>(0.0, 0.0, 0.0, 1.0);
      }
    `;

    const descriptor = {
      vertex: {
        module: vertex_mismatched
          ? t.mismatchedDevice.createShaderModule({ code })
          : t.device.createShaderModule({ code }),
        entryPoint: 'main',
      },

      fragment: {
        module: fragment_mismatched
          ? t.mismatchedDevice.createShaderModule({ code: t.getFragmentShaderCode('float', 4) })
          : t.device.createShaderModule({ code: t.getFragmentShaderCode('float', 4) }),
        entryPoint: 'main',
        targets: [{ format: 'rgba8unorm' }],
      },

      layout: t.getPipelineLayout(),
    };

    t.doCreateRenderPipelineTest(isAsync, _success, descriptor);
  });

g.test('entry_point_name_must_match')
  .desc(
    'TODO: Test the matching of entrypoint names for vertex and fragment (see the equivalent test for createComputePipeline).'
  )
  .unimplemented();
