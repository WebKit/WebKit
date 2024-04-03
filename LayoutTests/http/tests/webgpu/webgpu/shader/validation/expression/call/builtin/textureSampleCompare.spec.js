/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/const builtin = 'textureSampleCompare';export const description = `
Validation tests for the ${builtin}() builtin.

* test textureSampleCompare coords parameter must be correct type
* test textureSampleCompare array_index parameter must be correct type
* test textureSampleCompare depth_ref parameter must be correct type
* test textureSampleCompare offset parameter must be correct type
* test textureSampleCompare offset parameter must be a const-expression
* test textureSampleCompare offset parameter must be between -8 and +7 inclusive
* test textureSample not usable in a compute or vertex shader

note: uniformity validation is covered in src/webgpu/shader/validation/uniformity/uniformity.spec.ts
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf, objectsToRecord } from '../../../../../../common/util/data_tables.js';
import {
  Type,
  kAllScalarsAndVectors,
  isConvertible,


  isUnsignedType } from
'../../../../../util/conversion.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

import { kEntryPointsToValidateFragmentOnlyBuiltins } from './shader_stage_utils.js';







const kValidTextureSampleCompareParameterTypes = {
  texture_depth_2d: { coordsArgType: Type.vec2f, offsetArgType: Type.vec2i },
  texture_depth_2d_array: {
    coordsArgType: Type.vec2f,
    hasArrayIndexArg: true,
    offsetArgType: Type.vec2i
  },
  texture_depth_cube: { coordsArgType: Type.vec3f },
  texture_depth_cube_array: { coordsArgType: Type.vec3f, hasArrayIndexArg: true }
};

const kTextureTypes = keysOf(kValidTextureSampleCompareParameterTypes);
const kValuesTypes = objectsToRecord(kAllScalarsAndVectors);

export const g = makeTestGroup(ShaderValidationTest);

g.test('coords_argument').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#texturesamplecompare').
desc(
  `
Validates that only incorrect coords arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', keysOf(kValidTextureSampleCompareParameterTypes)).
combine('coordType', keysOf(kValuesTypes)).
beginSubcases().
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.coordType]) || t.value >= 0).
expand('offset', (t) =>
kValidTextureSampleCompareParameterTypes[t.textureType].offsetArgType ?
[false, true] :
[false]
)
).
fn((t) => {
  const { textureType, coordType, offset, value } = t.params;
  const coordArgType = kValuesTypes[coordType];
  const {
    offsetArgType,
    coordsArgType: coordsRequiredType,
    hasArrayIndexArg
  } = kValidTextureSampleCompareParameterTypes[textureType];

  const coordWGSL = coordArgType.create(value).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';
  const offsetWGSL = offset ? `, ${offsetArgType?.create(0).wgsl()}` : '';

  const code = `
@group(0) @binding(0) var s: sampler_comparison;
@group(0) @binding(1) var t: ${textureType};
@fragment fn fs() -> @location(0) vec4f {
  _ = textureSampleCompare(t, s, ${coordWGSL}${arrayWGSL}, 0${offsetWGSL});
  return vec4f(0);
}
`;
  const expectSuccess = isConvertible(coordArgType, coordsRequiredType);
  t.expectCompileResult(expectSuccess, code);
});

g.test('array_index_argument').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#texturesamplecompare').
desc(
  `
Validates that only incorrect array_index arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kTextureTypes)
// filter out types with no array_index
.filter((t) => !!kValidTextureSampleCompareParameterTypes[t.textureType].hasArrayIndexArg).
combine('arrayIndexType', keysOf(kValuesTypes)).
beginSubcases().
combine('value', [-9, -8, 0, 7, 8])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.arrayIndexType]) || t.value >= 0).
expand('offset', (t) =>
kValidTextureSampleCompareParameterTypes[t.textureType].offsetArgType ?
[false, true] :
[false]
)
).
fn((t) => {
  const { textureType, arrayIndexType, value, offset } = t.params;
  const arrayIndexArgType = kValuesTypes[arrayIndexType];
  const args = [arrayIndexArgType.create(value)];
  const { coordsArgType, offsetArgType } = kValidTextureSampleCompareParameterTypes[textureType];

  const coordWGSL = coordsArgType.create(0).wgsl();
  const arrayWGSL = args.map((arg) => arg.wgsl()).join(', ');
  const offsetWGSL = offset ? `, ${offsetArgType.create(0).wgsl()}` : '';

  const code = `
@group(0) @binding(0) var s: sampler_comparison;
@group(0) @binding(1) var t: ${textureType};
@fragment fn fs() -> @location(0) vec4f {
  _ = textureSampleCompare(t, s, ${coordWGSL}, ${arrayWGSL}, 0${offsetWGSL});
  return vec4f(0);
}
`;
  const expectSuccess =
  isConvertible(arrayIndexArgType, Type.i32) || isConvertible(arrayIndexArgType, Type.u32);
  t.expectCompileResult(expectSuccess, code);
});

g.test('depth_ref_argument').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#texturesamplecompare').
desc(
  `
Validates that only incorrect depth_ref arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kTextureTypes).
combine('depthRefType', keysOf(kValuesTypes)).
beginSubcases().
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.depthRefType]) || t.value >= 0).
expand('offset', (t) =>
kValidTextureSampleCompareParameterTypes[t.textureType].offsetArgType ?
[false, true] :
[false]
)
).
fn((t) => {
  const { textureType, depthRefType, value, offset } = t.params;
  const depthRefArgType = kValuesTypes[depthRefType];
  const args = [depthRefArgType.create(value)];
  const { coordsArgType, hasArrayIndexArg, offsetArgType } =
  kValidTextureSampleCompareParameterTypes[textureType];

  const coordWGSL = coordsArgType.create(0).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';
  const depthRefWGSL = args.map((arg) => arg.wgsl()).join(', ');
  const offsetWGSL = offset ? `, ${offsetArgType.create(0).wgsl()}` : '';

  const code = `
@group(0) @binding(0) var s: sampler_comparison;
@group(0) @binding(1) var t: ${textureType};
@fragment fn fs() -> @location(0) vec4f {
  _ = textureSampleCompare(t, s, ${coordWGSL}${arrayWGSL}, ${depthRefWGSL}${offsetWGSL});
  return vec4f(0);
}
`;
  const expectSuccess = isConvertible(depthRefArgType, Type.f32);
  t.expectCompileResult(expectSuccess, code);
});

g.test('offset_argument').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#texturesamplecompare').
desc(
  `
Validates that only incorrect offset arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kTextureTypes)
// filter out types with no offset
.filter((t) => !!kValidTextureSampleCompareParameterTypes[t.textureType].offsetArgType).
combine('offsetType', keysOf(kValuesTypes)).
beginSubcases().
combine('value', [-9, -8, 0, 7, 8])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.offsetType]) || t.value >= 0)
).
fn((t) => {
  const { textureType, offsetType, value } = t.params;
  const offsetArgType = kValuesTypes[offsetType];
  const args = [offsetArgType.create(value)];
  const {
    coordsArgType,
    hasArrayIndexArg,
    offsetArgType: offsetRequiredType
  } = kValidTextureSampleCompareParameterTypes[textureType];

  const coordWGSL = coordsArgType.create(0).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';
  const offsetWGSL = args.map((arg) => arg.wgsl()).join(', ');

  const code = `
@group(0) @binding(0) var s: sampler_comparison;
@group(0) @binding(1) var t: ${textureType};
@fragment fn fs() -> @location(0) vec4f {
  _ = textureSampleCompare(t, s, ${coordWGSL}${arrayWGSL}, 0, ${offsetWGSL});
  return vec4f(0);
}
`;
  const expectSuccess =
  isConvertible(offsetArgType, offsetRequiredType) && value >= -8 && value <= 7;
  t.expectCompileResult(expectSuccess, code);
});

g.test('offset_argument,non_const').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#texturesamplecompare').
desc(
  `
Validates that only non-const offset arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kTextureTypes).
combine('varType', ['c', 'u', 'l'])
// filter out types with no offset
.filter((t) => !!kValidTextureSampleCompareParameterTypes[t.textureType].offsetArgType)
).
fn((t) => {
  const { textureType, varType } = t.params;
  const { coordsArgType, hasArrayIndexArg, offsetArgType } =
  kValidTextureSampleCompareParameterTypes[textureType];

  const coordWGSL = coordsArgType.create(0).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';
  const offsetWGSL = `${offsetArgType}(${varType})`;

  const code = `
@group(0) @binding(0) var s: sampler_comparison;
@group(0) @binding(1) var t: ${textureType};
@group(0) @binding(2) var<uniform> u: ${offsetArgType};
@fragment fn fs() -> @location(0) vec4f {
  const c = 1;
  let l = ${offsetArgType?.create(0).wgsl()};
  _ = textureSampleCompare(t, s, ${coordWGSL}${arrayWGSL}, 0, ${offsetWGSL});
  return vec4f(0);
}
`;
  const expectSuccess = varType === 'c';
  t.expectCompileResult(expectSuccess, code);
});

g.test('only_in_fragment').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#texturesample').
desc(
  `
Validates that ${builtin} must not be used in a compute or vertex shader.
`
).
params((u) =>
u.
combine('textureType', kTextureTypes).
combine('entryPoint', keysOf(kEntryPointsToValidateFragmentOnlyBuiltins)).
expand('offset', (t) =>
kValidTextureSampleCompareParameterTypes[t.textureType].offsetArgType ?
[false, true] :
[false]
)
).
fn((t) => {
  const { textureType, entryPoint, offset } = t.params;
  const { coordsArgType, hasArrayIndexArg, offsetArgType } =
  kValidTextureSampleCompareParameterTypes[textureType];

  const coordWGSL = coordsArgType.create(0).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';
  const offsetWGSL = offset ? `, ${offsetArgType?.create(0).wgsl()}` : '';

  const config = kEntryPointsToValidateFragmentOnlyBuiltins[entryPoint];
  const code = `
${config.code}
@group(0) @binding(0) var s: sampler_comparison;
@group(0) @binding(1) var t: ${textureType};

fn foo() {
  _ = textureSampleCompare(t, s, ${coordWGSL}${arrayWGSL}, 0${offsetWGSL});
}`;
  t.expectCompileResult(config.expectSuccess, code);
});