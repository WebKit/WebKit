/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/const builtin = 'textureSampleBaseClampToEdge';export const description = `
Validation tests for the ${builtin}() builtin.

* test textureSampleBaseClampToEdge coords parameter must be correct type
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

const kTextureSampleBaseClampToEdgeTextureTypes = ['texture_2d<f32>', 'texture_external'];
const kValuesTypes = objectsToRecord(kAllScalarsAndVectors);

export const g = makeTestGroup(ShaderValidationTest);

g.test('coords_argument').
specURL('https://gpuweb.github.io/gpuweb/wgsl/#texturesamplebaseclamptoedge').
desc(
  `
Validates that only incorrect coords arguments are rejected by ${builtin}
`
).
params((u) =>
u.
combine('textureType', kTextureSampleBaseClampToEdgeTextureTypes).
combine('coordType', keysOf(kValuesTypes)).
beginSubcases().
combine('value', [-1, 0, 1])
// filter out unsigned types with negative values
.filter((t) => !isUnsignedType(kValuesTypes[t.coordType]) || t.value >= 0)
).
fn((t) => {
  const { textureType, coordType, value } = t.params;
  const coordArgType = kValuesTypes[coordType];
  const coordWGSL = coordArgType.create(value).wgsl();

  const code = `
@group(0) @binding(0) var s: sampler;
@group(0) @binding(1) var t: ${textureType};
@fragment fn fs() -> @location(0) vec4f {
  let v = textureSampleBaseClampToEdge(t, s, ${coordWGSL});
  return vec4f(0);
}
`;
  const expectSuccess = isConvertible(coordArgType, Type.vec2f);
  t.expectCompileResult(expectSuccess, code);
});