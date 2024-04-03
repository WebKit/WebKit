/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/const builtin = 'textureLoad';export const description = `
Validation tests for the ${builtin}() builtin.

* test textureLoad coords parameter must be correct type
* test textureLoad array_index parameter must be correct type
* test textureLoad level parameter must be correct type
* test textureLoad sample_index parameter must be correct type
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf, objectsToRecord } from '../../../../../../common/util/data_tables.js';
import { assert } from '../../../../../../common/util/util.js';
import { kAllTextureFormats, kTextureFormatInfo } from '../../../../../format_info.js';
import {
  Type,
  kAllScalarsAndVectors,
  isConvertible,


  isUnsignedType } from
'../../../../../util/conversion.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';









const kCoords1DTypes = [Type.i32, Type.u32];
const kCoords2DTypes = [Type.vec2i, Type.vec2u];
const kCoords3DTypes = [Type.vec3i, Type.vec3u];

const kValidTextureLoadParameterTypesForNonStorageTextures =
{
  texture_1d: { usesMultipleTypes: true, coordsArgTypes: kCoords1DTypes, hasLevelArg: true },
  texture_2d: { usesMultipleTypes: true, coordsArgTypes: kCoords2DTypes, hasLevelArg: true },
  texture_2d_array: {
    usesMultipleTypes: true,
    coordsArgTypes: kCoords2DTypes,
    hasArrayIndexArg: true,
    hasLevelArg: true
  },
  texture_3d: { usesMultipleTypes: true, coordsArgTypes: kCoords3DTypes, hasLevelArg: true },
  texture_multisampled_2d: {
    usesMultipleTypes: true,
    coordsArgTypes: kCoords2DTypes,
    hasSampleIndexArg: true
  },
  texture_depth_2d: { coordsArgTypes: kCoords2DTypes, hasLevelArg: true },
  texture_depth_2d_array: {
    coordsArgTypes: kCoords2DTypes,
    hasArrayIndexArg: true,
    hasLevelArg: true
  },
  texture_depth_multisampled_2d: { coordsArgTypes: kCoords2DTypes, hasSampleIndexArg: true },
  texture_external: { coordsArgTypes: kCoords2DTypes }
};

const kValidTextureLoadParameterTypesForStorageTextures = {
  texture_storage_1d: { coordsArgTypes: [Type.i32, Type.u32] },
  texture_storage_2d: { coordsArgTypes: [Type.vec2i, Type.vec2u] },
  texture_storage_2d_array: {
    coordsArgTypes: [Type.vec2i, Type.vec2u],
    hasArrayIndexArg: true
  },
  texture_storage_3d: { coordsArgTypes: [Type.vec3i, Type.vec3u] }
};

const kNonStorageTextureTypes = keysOf(kValidTextureLoadParameterTypesForNonStorageTextures);
const kStorageTextureTypes = keysOf(kValidTextureLoadParameterTypesForStorageTextures);
const kValuesTypes = objectsToRecord(kAllScalarsAndVectors);

const kTexelType = {
  f32: Type.vec4f,
  i32: Type.vec4i,
  u32: Type.vec4u
};

const kTexelTypes = keysOf(kTexelType);

export const g = makeTestGroup(ShaderValidationTest);

g.test('coords_argument,non_storage').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#textureload').
desc(
  `
Validates that only incorrect coords arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kNonStorageTextureTypes).
combine('coordType', keysOf(kValuesTypes)).
beginSubcases().
expand('texelType', (t) =>
kValidTextureLoadParameterTypesForNonStorageTextures[t.textureType].usesMultipleTypes ?
kTexelTypes :
['']
).
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.coordType]) || t.value >= 0)
).
fn((t) => {
  const { textureType, coordType, texelType, value } = t.params;
  const coordArgType = kValuesTypes[coordType];
  const { coordsArgTypes, hasArrayIndexArg, hasLevelArg, hasSampleIndexArg } =
  kValidTextureLoadParameterTypesForNonStorageTextures[textureType];

  const texelTypeWGSL = texelType ? `<${texelType}>` : '';
  const coordWGSL = coordArgType.create(value).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';
  const levelWGSL = hasLevelArg ? ', 0' : '';
  const sampleIndexWGSL = hasSampleIndexArg ? ', 0' : '';

  const code = `
@group(0) @binding(0) var t: ${textureType}${texelTypeWGSL};
@fragment fn fs() -> @location(0) vec4f {
  _ = textureLoad(t, ${coordWGSL}${arrayWGSL}${levelWGSL}${sampleIndexWGSL});
  return vec4f(0);
}
`;
  const expectSuccess =
  isConvertible(coordArgType, coordsArgTypes[0]) ||
  isConvertible(coordArgType, coordsArgTypes[1]);
  t.expectCompileResult(expectSuccess, code);
});

g.test('coords_argument,storage').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#textureload').
desc(
  `
Validates that only incorrect coords arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kStorageTextureTypes).
combine('coordType', keysOf(kValuesTypes)).
beginSubcases().
combine('format', kAllTextureFormats)
// filter to only storage texture formats.
.filter((t) => !!kTextureFormatInfo[t.format].color?.storage).
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.coordType]) || t.value >= 0)
).
beforeAllSubcases((t) =>
t.skipIfLanguageFeatureNotSupported('readonly_and_readwrite_storage_textures')
).
fn((t) => {
  const { textureType, coordType, format, value } = t.params;
  const coordArgType = kValuesTypes[coordType];
  const { coordsArgTypes, hasArrayIndexArg } =
  kValidTextureLoadParameterTypesForStorageTextures[textureType];

  const coordWGSL = coordArgType.create(value).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';

  const code = `
@group(0) @binding(0) var t: ${textureType}<${format}, read>;
@fragment fn fs() -> @location(0) vec4f {
  _ = textureLoad(t, ${coordWGSL}${arrayWGSL});
  return vec4f(0);
}
`;
  const expectSuccess =
  isConvertible(coordArgType, coordsArgTypes[0]) ||
  isConvertible(coordArgType, coordsArgTypes[1]);
  t.expectCompileResult(expectSuccess, code);
});

g.test('array_index_argument,non_storage').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#textureload').
desc(
  `
Validates that only incorrect array_index arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kNonStorageTextureTypes)
// filter out types with no array_index
.filter(
  (t) => !!kValidTextureLoadParameterTypesForNonStorageTextures[t.textureType].hasArrayIndexArg
).
combine('arrayIndexType', keysOf(kValuesTypes)).
beginSubcases().
expand('texelType', (t) =>
kValidTextureLoadParameterTypesForNonStorageTextures[t.textureType].usesMultipleTypes ?
kTexelTypes :
['']
).
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.arrayIndexType]) || t.value >= 0)
).
fn((t) => {
  const { textureType, arrayIndexType, texelType, value } = t.params;
  const arrayIndexArgType = kValuesTypes[arrayIndexType];
  const args = [arrayIndexArgType.create(value)];
  const { coordsArgTypes, hasLevelArg } =
  kValidTextureLoadParameterTypesForNonStorageTextures[textureType];

  const texelTypeWGSL = texelType ? `<${texelType}>` : '';
  const coordWGSL = coordsArgTypes[0].create(0).wgsl();
  const arrayWGSL = args.map((arg) => arg.wgsl()).join(', ');
  const levelWGSL = hasLevelArg ? ', 0' : '';

  const code = `
@group(0) @binding(0) var t: ${textureType}${texelTypeWGSL};
@fragment fn fs() -> @location(0) vec4f {
  _ = textureLoad(t, ${coordWGSL}, ${arrayWGSL}${levelWGSL});
  return vec4f(0);
}
`;
  const expectSuccess =
  isConvertible(arrayIndexArgType, Type.i32) || isConvertible(arrayIndexArgType, Type.u32);
  t.expectCompileResult(expectSuccess, code);
});

g.test('array_index_argument,storage').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#textureload').
desc(
  `
Validates that only incorrect array_index arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kStorageTextureTypes)
// filter out types with no array_index
.filter(
  (t) => !!kValidTextureLoadParameterTypesForStorageTextures[t.textureType].hasArrayIndexArg
).
combine('arrayIndexType', keysOf(kValuesTypes)).
beginSubcases().
combine('format', kAllTextureFormats)
// filter to only storage texture formats.
.filter((t) => !!kTextureFormatInfo[t.format].color?.storage).
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.arrayIndexType]) || t.value >= 0)
).
beforeAllSubcases((t) =>
t.skipIfLanguageFeatureNotSupported('readonly_and_readwrite_storage_textures')
).
fn((t) => {
  const { textureType, arrayIndexType, format, value } = t.params;
  const arrayIndexArgType = kValuesTypes[arrayIndexType];
  const args = [arrayIndexArgType.create(value)];
  const { coordsArgTypes, hasLevelArg } =
  kValidTextureLoadParameterTypesForStorageTextures[textureType];

  const coordWGSL = coordsArgTypes[0].create(0).wgsl();
  const arrayWGSL = args.map((arg) => arg.wgsl()).join(', ');
  const levelWGSL = hasLevelArg ? ', 0' : '';

  const code = `
@group(0) @binding(0) var t: ${textureType}<${format}, read>;
@fragment fn fs() -> @location(0) vec4f {
  _ = textureLoad(t, ${coordWGSL}, ${arrayWGSL}${levelWGSL});
  return vec4f(0);
}
`;
  const expectSuccess =
  isConvertible(arrayIndexArgType, Type.i32) || isConvertible(arrayIndexArgType, Type.u32);
  t.expectCompileResult(expectSuccess, code);
});

g.test('level_argument,non_storage').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#textureload').
desc(
  `
Validates that only incorrect level arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kNonStorageTextureTypes)
// filter out types with no level
.filter(
  (t) => !!kValidTextureLoadParameterTypesForNonStorageTextures[t.textureType].hasLevelArg
).
combine('levelType', keysOf(kValuesTypes)).
beginSubcases().
expand('texelType', (t) =>
kValidTextureLoadParameterTypesForNonStorageTextures[t.textureType].usesMultipleTypes ?
kTexelTypes :
['']
).
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.levelType]) || t.value >= 0)
).
fn((t) => {
  const { textureType, levelType, texelType, value } = t.params;
  const levelArgType = kValuesTypes[levelType];
  const { coordsArgTypes, hasArrayIndexArg } =
  kValidTextureLoadParameterTypesForNonStorageTextures[textureType];

  const texelTypeWGSL = texelType ? `<${texelType}>` : '';
  const coordWGSL = coordsArgTypes[0].create(0).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';
  const levelWGSL = levelArgType.create(value).wgsl();

  const code = `
@group(0) @binding(0) var t: ${textureType}${texelTypeWGSL};
@fragment fn fs() -> @location(0) vec4f {
  _ = textureLoad(t, ${coordWGSL}${arrayWGSL}, ${levelWGSL});
  return vec4f(0);
}
`;
  const expectSuccess =
  isConvertible(levelArgType, Type.i32) || isConvertible(levelArgType, Type.u32);
  t.expectCompileResult(expectSuccess, code);
});

g.test('sample_index_argument,non_storage').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#textureload').
desc(
  `
Validates that only incorrect sample_index arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kNonStorageTextureTypes)
// filter out types with no sample_index
.filter(
  (t) => !!kValidTextureLoadParameterTypesForNonStorageTextures[t.textureType].hasSampleIndexArg
).
combine('sampleIndexType', keysOf(kValuesTypes)).
beginSubcases().
expand('texelType', (t) =>
kValidTextureLoadParameterTypesForNonStorageTextures[t.textureType].usesMultipleTypes ?
kTexelTypes :
['']
).
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.sampleIndexType]) || t.value >= 0)
).
fn((t) => {
  const { textureType, sampleIndexType, texelType, value } = t.params;
  const sampleIndexArgType = kValuesTypes[sampleIndexType];
  const { coordsArgTypes, hasArrayIndexArg, hasLevelArg } =
  kValidTextureLoadParameterTypesForNonStorageTextures[textureType];
  assert(!hasLevelArg);

  const texelTypeWGSL = texelType ? `<${texelType}>` : '';
  const coordWGSL = coordsArgTypes[0].create(0).wgsl();
  const arrayWGSL = hasArrayIndexArg ? ', 0' : '';
  const sampleIndexWGSL = sampleIndexArgType.create(value).wgsl();

  const code = `
@group(0) @binding(0) var t: ${textureType}${texelTypeWGSL};
@fragment fn fs() -> @location(0) vec4f {
  _ = textureLoad(t, ${coordWGSL}${arrayWGSL}, ${sampleIndexWGSL});
  return vec4f(0);
}
`;
  const expectSuccess =
  isConvertible(sampleIndexArgType, Type.i32) || isConvertible(sampleIndexArgType, Type.u32);
  t.expectCompileResult(expectSuccess, code);
});