/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
This test dedicatedly tests validation of GPUMultisampleState of createRenderPipeline.
`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import {
  kPossibleColorRenderableTextureFormats,
  isColorTextureFormatWithAlpha,
  isTextureFormatBlendable } from
'../../../format_info.js';
import { kDefaultVertexShaderCode, kDefaultFragmentShaderCode } from '../../../util/shader.js';
import * as vtu from '../validation_test_utils.js';

import { CreateRenderPipelineValidationTest } from './common.js';

export const g = makeTestGroup(CreateRenderPipelineValidationTest);

g.test('count').
desc(`If multisample.count must either be 1 or 4.`).
params((u) =>
u.
combine('isAsync', [false, true]).
beginSubcases().
combine('count', [0, 1, 2, 3, 4, 8, 16, 1024])
).
fn((t) => {
  const { isAsync, count } = t.params;

  const descriptor = t.getDescriptor({ multisample: { count, alphaToCoverageEnabled: false } });

  const _success = count === 1 || count === 4;
  vtu.doCreateRenderPipelineTest(t, isAsync, _success, descriptor);
});

g.test('alpha_to_coverage,count').
desc(
  `If multisample.alphaToCoverageEnabled is true, multisample.count must be greater than 1, e.g. it can only be 4.`
).
params((u) =>
u.
combine('isAsync', [false, true]).
combine('alphaToCoverageEnabled', [false, true]).
beginSubcases().
combine('count', [1, 4])
).
fn((t) => {
  const { isAsync, alphaToCoverageEnabled, count } = t.params;

  const descriptor = t.getDescriptor({ multisample: { count, alphaToCoverageEnabled } });

  const _success = alphaToCoverageEnabled ? count === 4 : count === 1 || count === 4;
  vtu.doCreateRenderPipelineTest(t, isAsync, _success, descriptor);
});

g.test('alpha_to_coverage,fragment_stage_required').
desc(`If multisample.alphaToCoverageEnabled is true, a fragment stage must be provided.`).
params((u) =>
u.
combine('isAsync', [false, true]).
combine('alphaToCoverageEnabled', [false, true]).
beginSubcases().
combine('noFragment', [false, true])
).
fn((t) => {
  const { isAsync, alphaToCoverageEnabled, noFragment } = t.params;

  const descriptor = t.getDescriptor({
    multisample: { alphaToCoverageEnabled, count: 4 },
    noFragment,
    depthStencil: { format: 'depth32float', depthWriteEnabled: false }
  });

  const _success = !(alphaToCoverageEnabled && noFragment);
  vtu.doCreateRenderPipelineTest(t, isAsync, _success, descriptor);
});

g.test('alpha_to_coverage,first_target_required').
desc(`If multisample.alphaToCoverageEnabled is true, a fragment.targets[0] must exist.`).
params((u) =>
u.
combine('isAsync', [false, true]).
combine('alphaToCoverageEnabled', [false, true]).
beginSubcases().
combine('targetIndex', [0, 1])
).
fn((t) => {
  const { isAsync, alphaToCoverageEnabled, targetIndex } = t.params;

  const targets = [];
  targets[targetIndex] = { format: 'rgba8unorm' };

  const descriptor = {
    vertex: { module: t.device.createShaderModule({ code: kDefaultVertexShaderCode }) },
    fragment: {
      module: t.device.createShaderModule({
        code: `
                    @fragment fn fs() -> @location(${targetIndex}) vec4f {
                        return vec4f();
                    }
                `
      }),
      targets
    },
    multisample: { alphaToCoverageEnabled, count: 4 },
    layout: 'auto'
  };

  const _success = !(alphaToCoverageEnabled && targetIndex === 1);
  vtu.doCreateRenderPipelineTest(t, isAsync, _success, descriptor);
});

g.test('alpha_to_coverage,first_format_blendable_and_has_alpha').
desc(
  `If multisample.alphaToCoverageEnabled is true, a fragment.targets[0].format must be blendable.`
).
params((u) =>
u.
combine('isAsync', [false, true]).
combine('alphaToCoverageEnabled', [false, true]).
beginSubcases().
combine('format', kPossibleColorRenderableTextureFormats)
).
fn((t) => {
  const { isAsync, alphaToCoverageEnabled, format } = t.params;
  t.skipIfTextureFormatNotSupported(format);
  t.skipIfTextureFormatNotUsableAsRenderAttachment(format);

  const descriptor = t.getDescriptor({
    targets: [{ format }],
    multisample: { alphaToCoverageEnabled, count: 4 }
  });

  const blendableWithAlpha =
  isTextureFormatBlendable(t.device.features, format) && isColorTextureFormatWithAlpha(format);
  const _success = !alphaToCoverageEnabled || blendableWithAlpha;
  vtu.doCreateRenderPipelineTest(t, isAsync, _success, descriptor);
});

g.test('alpha_to_coverage,sample_mask').
desc(
  `If sample_mask builtin is a pipeline output of fragment, multisample.alphaToCoverageEnabled should be false.`
).
params((u) =>
u.
combine('isAsync', [false, true]).
combine('alphaToCoverageEnabled', [false, true]).
beginSubcases().
combine('hasSampleMaskOutput', [false, true])
).
fn((t) => {
  const { isAsync, alphaToCoverageEnabled, hasSampleMaskOutput } = t.params;

  if (t.isCompatibility && hasSampleMaskOutput) {
    t.skip('WGSL sample_mask is not supported in compatibility mode');
  }

  const descriptor = t.getDescriptor({
    multisample: { alphaToCoverageEnabled, count: 4 },
    fragmentShaderCode: hasSampleMaskOutput ?
    `
      struct Output {
        @builtin(sample_mask) mask_out: u32,
        @location(0) color : vec4<f32>,
      }
      @fragment fn main() -> Output {
        var o: Output;
        // We need to make sure this sample_mask isn't optimized out even its value equals "no op".
        o.mask_out = 0xFFFFFFFFu;
        o.color = vec4<f32>(1.0, 1.0, 1.0, 1.0);
        return o;
      }` :
    kDefaultFragmentShaderCode
  });

  const _success = !hasSampleMaskOutput || !alphaToCoverageEnabled;
  vtu.doCreateRenderPipelineTest(t, isAsync, _success, descriptor);
});