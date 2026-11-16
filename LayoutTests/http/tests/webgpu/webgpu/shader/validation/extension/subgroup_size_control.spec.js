/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for the subgroup_size_control extension
`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { isPowerOfTwo } from '../../../util/math.js';
import { UniqueFeaturesAndLimitsShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(UniqueFeaturesAndLimitsShaderValidationTest);

g.test('enable_subgroup_size_control_requires_subgroups').
desc(
  `Checks that enabling the WGSL extension subgroup_size_control also enables the
     subgroups extension.`
).
params((u) => u.combine('enableSubgroups', [false, true])).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn((t) => {
  const { enableSubgroups } = t.params;

  t.expectCompileResult(
    true,
    `
        ${enableSubgroups ? 'enable subgroups;' : ''}
        enable subgroup_size_control;
        @group(0) @binding(0) var<storage, read_write> out : u32;
        @compute @workgroup_size(1)
        fn main(@builtin(subgroup_size) size : u32) {
          out = size;
        }
      `
  );
});

g.test('use_subgroup_size_attribute_requires_subgroup_size_control_extension_enabled').
desc(
  `Checks that the @subgroup_size attribute is only allowed with the WGSL extension
     subgroup_size_control enabled in the shader and the WebGPU extension subgroup-size-control
     supported on the device.`
).
params((u) => u.combine('enableExtension', [false, true])).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn((t) => {
  const { enableExtension } = t.params;

  const kSubgroupSize = 4;
  t.expectCompileResult(
    enableExtension,
    `
        enable subgroups;
        ${enableExtension ? 'enable subgroup_size_control;' : ''}
        @compute @workgroup_size(${kSubgroupSize}) @subgroup_size(${kSubgroupSize})
        fn main() {}
      `
  );
});

g.test('subgroup_size_attribute_only_valid_in_compute_stage').
desc(
  `Checks that the @subgroup_size attribute is only valid on a compute shader entry point.
     Applying it to a vertex or fragment entry point must be a compilation error.`
).
params((u) => u.combine('stage', ['compute', 'vertex', 'fragment'])).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn((t) => {
  const { stage } = t.params;
  const kSubgroupSize = 4;

  const kStageShaders = {
    compute: `
    enable subgroups;
    enable subgroup_size_control;
    @compute @workgroup_size(${kSubgroupSize}) @subgroup_size(${kSubgroupSize})
    fn main() {}
  `,
    vertex: `
    enable subgroups;
    enable subgroup_size_control;
    @vertex @subgroup_size(${kSubgroupSize})
    fn main() -> @builtin(position) vec4f {
      return vec4f(0);
    }
  `,
    fragment: `
    enable subgroups;
    enable subgroup_size_control;
    @fragment @subgroup_size(${kSubgroupSize})
    fn main() -> @location(0) vec4f {
      return vec4f(0);
    }
  `
  };

  t.expectCompileResult(stage === 'compute', kStageShaders[stage]);
});

const kSubgroupSizeValueCases = {
  literal_abstract_int: { expr: '4', decl: '', pass: true },
  literal_u32: { expr: '4u', decl: '', pass: true },
  literal_i32: { expr: '4i', decl: '', pass: true },
  const_i32: { expr: 'k_i32', decl: 'const k_i32: i32 = 4;', pass: true },
  const_u32: { expr: 'k_u32', decl: 'const k_u32: u32 = 4;', pass: true },
  const_expr_abstract: { expr: '2 + 2', decl: '', pass: true },
  const_expr_named: { expr: 'k + 1', decl: 'const k = 3;', pass: true },
  override_i32: { expr: 'o_i32', decl: 'override o_i32: i32 = 4;', pass: true },
  override_u32: { expr: 'o_u32', decl: 'override o_u32: u32 = 4;', pass: true },
  override_expr: { expr: 'o + 1', decl: 'override o: u32 = 3;', pass: true },
  literal_f32: { expr: '4.0f', decl: '', pass: false },
  literal_abstract_float: { expr: '4.0', decl: '', pass: false },
  literal_bool: { expr: 'true', decl: '', pass: false },
  const_f32: { expr: 'k_f32', decl: 'const k_f32: f32 = 4.0;', pass: false },
  const_bool: { expr: 'k_bool', decl: 'const k_bool: bool = true;', pass: false },
  override_f32: { expr: 'o_f32', decl: 'override o_f32: f32 = 4.0;', pass: false },
  let_u32: { expr: 'r', decl: 'fn dummy() -> u32 { let r: u32 = 4; return r; }', pass: false },
  var_u32: { expr: 'v', decl: 'var<private> v: u32 = 4;', pass: false }
};

g.test('subgroup_size_value_must_be_const_or_override_i32_u32').
desc(
  `Checks that the value of @subgroup_size must be a constant expression or an override
     expression that resolves to an i32 or a u32.`
).
params((u) => u.combine('case', keysOf(kSubgroupSizeValueCases))).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn((t) => {
  const { expr, decl, pass } = kSubgroupSizeValueCases[t.params.case];

  t.expectCompileResult(
    pass,
    `
        enable subgroups;
        enable subgroup_size_control;
        ${decl}
        @compute @workgroup_size(4) @subgroup_size(${expr})
        fn main() {}
      `
  );
});

g.test('subgroup_size_constant_value_must_be_power_of_2').
desc(
  `Checks that when @subgroup_size is a constant expression, it is a shader creation error if
    the value is not a power of 2.`
).
params((u) =>
u.combine('size', [0, 1, 2, 3, 4, 5, 8, 9, 15, 16, 32, 33, 48, 64, 65, 100, 128, 256])
).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn((t) => {
  const { size } = t.params;

  t.expectCompileResult(
    isPowerOfTwo(size),
    `
        enable subgroups;
        enable subgroup_size_control;
        @compute @workgroup_size(${size}) @subgroup_size(${size})
        fn main() {}
      `
  );
});

g.test('subgroup_size_override_must_be_power_of_2_at_pipeline_creation').
desc(
  `Checks that when @subgroup_size is an override expression, it is a pipeline creation error
     if the override value resolves to a value that is not a power of 2.`
).
params((u) => u.combine('size', [0, 3, 5, 7, 15, 31, 63, 127])).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn((t) => {
  const { size } = t.params;

  t.expectPipelineResult({
    expectedResult: false,
    code: `
        enable subgroups;
        enable subgroup_size_control;
        override S: u32;
        @workgroup_size(S) @subgroup_size(S)`,
    constants: { S: size },
    addWorkgroupSize: false
  });
});

/**
 * Returns all valid subgroup sizes for the given adapter info, i.e. all power-of-two values
 * between subgroupMinSize and subgroupMaxSize inclusive.
 */
async function getValidSubgroupSizes(device) {
  const subgroupMinSize = device.adapterInfo.subgroupMinSize;
  const subgroupMaxSize = device.adapterInfo.subgroupMaxSize;
  const maxWorkgroupSizeX = device.limits.maxComputeWorkgroupSizeX;

  const sizes = [];
  for (let subgroupSize = subgroupMinSize; subgroupSize <= subgroupMaxSize; subgroupSize *= 2) {
    if (subgroupSize > maxWorkgroupSizeX) break;
    const wgsl = `
enable subgroups;
enable subgroup_size_control;

@compute @workgroup_size(${subgroupSize}, 1, 1) @subgroup_size(${subgroupSize})
fn main(@builtin(local_invocation_index) lid : u32) {
}`;
    device.pushErrorScope('validation');
    const module = device.createShaderModule({ code: wgsl });
    device.createComputePipeline({
      layout: 'auto',
      compute: { module, entryPoint: 'main' }
    });
    const error = await device.popErrorScope();
    if (error) {
      continue;
    }
    sizes.push(subgroupSize);
  }
  return sizes;
}

g.test('subgroup_size_override_valid_values_no_error').
desc(
  `Checks that when @subgroup_size is an override expression and the override value resolves
     to a valid subgroup size (a power of 2 between subgroupMinSize and subgroupMaxSize), pipeline
     creation succeeds without error.`
).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn(async (t) => {
  const validSubgroupSizes = await getValidSubgroupSizes(t.device);
  t.expect(validSubgroupSizes.length > 0, 'Expected at least one valid subgroup size');

  for (const subgroupSize of validSubgroupSizes) {
    t.expectPipelineResult({
      expectedResult: true,
      code: `
          enable subgroups;
          enable subgroup_size_control;
          override S: u32;
          @workgroup_size(S) @subgroup_size(S)`,
      constants: { S: subgroupSize },
      addWorkgroupSize: false
    });
  }
});

g.test('workgroup_size_x_must_be_multiple_of_subgroup_size_at_pipeline_creation').
desc(
  `Checks that a pipeline-creation error results if the x-dimension of the entry point's
     workgroup_size is not a multiple of the subgroup_size value. Tests all combinations of
     constant and override expressions for both workgroup_size and subgroup_size.`
).
params((u) =>
u.
combine('workgroupSizeIsOverride', [false, true]).
combine('subgroupSizeIsOverride', [false, true]).
combine('offset', [0, 1, -1]).
combine('multiplier', [1, 2, 3])
).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn(async (t) => {
  const { workgroupSizeIsOverride, subgroupSizeIsOverride, offset, multiplier } = t.params;
  const validSubgroupSizes = await getValidSubgroupSizes(t.device);
  t.expect(validSubgroupSizes.length > 0, 'Expected at least one valid subgroup size');

  for (const subgroupSize of validSubgroupSizes) {
    const workgroupSizeX = subgroupSize * multiplier + offset;
    if (workgroupSizeX <= 0) continue;
    if (workgroupSizeX > t.device.limits.maxComputeWorkgroupSizeX) continue;

    const isMultiple = workgroupSizeX % subgroupSize === 0;

    t.expectPipelineResult({
      expectedResult: isMultiple,
      code: `
            enable subgroups;
            enable subgroup_size_control;
            const const_S = ${subgroupSize}u;
            const const_W = ${workgroupSizeX}u;
            override override_S: u32;
            override override_W: u32;
            @workgroup_size(${workgroupSizeIsOverride ? 'override_W' : 'const_W'})
            @subgroup_size(${subgroupSizeIsOverride ? 'override_S' : 'const_S'})
          `,
      constants: { override_S: subgroupSize, override_W: workgroupSizeX },
      addWorkgroupSize: false
    });
  }
});

g.test('subgroup_size_must_be_between_min_and_max_at_pipeline_creation').
desc(
  `Checks that a pipeline-creation error results if the subgroup_size value is smaller than
     subgroupMinSize or greater than subgroupMaxSize. Tests both constant and override expressions
     for the subgroup_size attribute.`
).
params((u) =>
u.
combine('subgroupSizeIsOverride', [false, true]).
combine('relation', ['below_min', 'at_min', 'at_max', 'above_max'])
).
beforeAllSubcases((t) => {
  t.selectDeviceOrSkipTestCase({
    requiredFeatures: ['subgroup-size-control']
  });
}).
fn((t) => {
  const { subgroupSizeIsOverride, relation } = t.params;
  const subgroupMinSize = t.device.adapterInfo.subgroupMinSize;
  const subgroupMaxSize = t.device.adapterInfo.subgroupMaxSize;

  let subgroupSize;
  let shouldSucceed;
  switch (relation) {
    case 'below_min':
      // Use a power-of-2 value below min. If min is already 1, skip.
      subgroupSize = subgroupMinSize / 2;
      if (subgroupSize < 1) {
        t.skip('subgroupMinSize is 1, cannot test below minimum');
        return;
      }
      shouldSucceed = false;
      break;
    case 'at_min':
      subgroupSize = subgroupMinSize;
      shouldSucceed = true;
      break;
    case 'at_max':
      subgroupSize = subgroupMaxSize;
      shouldSucceed = true;
      break;
    case 'above_max':
      // Use a power-of-2 value above max.
      subgroupSize = subgroupMaxSize * 2;
      shouldSucceed = false;
      break;
  }

  // Ensure workgroup_size x is a multiple of subgroup_size and within limits.
  const workgroupSizeX = subgroupSize;
  if (workgroupSizeX > t.device.limits.maxComputeWorkgroupSizeX) {
    t.skip('subgroup size exceeds maxComputeWorkgroupSizeX');
    return;
  }

  t.expectPipelineResult({
    expectedResult: shouldSucceed,
    code: `
          enable subgroups;
          enable subgroup_size_control;
          const const_S = ${subgroupSize}u;
          override override_S: u32;
          @workgroup_size(${workgroupSizeX})
          @subgroup_size(${subgroupSizeIsOverride ? 'override_S' : 'const_S'})
        `,
    constants: { override_S: subgroupSize },
    addWorkgroupSize: false
  });
});