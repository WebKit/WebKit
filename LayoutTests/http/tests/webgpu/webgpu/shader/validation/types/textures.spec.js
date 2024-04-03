/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for various texture types in shaders.

TODO:
- Sampled Texture Types
- Multisampled Texture Types
- External Sampled Texture Types
- Depth Texture Types
- Sampler Types
`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import {
  isTextureFormatUsableAsStorageFormat,
  kAllTextureFormats,
  kColorTextureFormats,
  kTextureFormatInfo } from
'../../../format_info.js';
import { getPlainTypeInfo } from '../../../util/shader.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

g.test('texel_formats').
desc(
  'Test channels and channel format of various texel formats when using as the storage texture format'
).
params((u) =>
u.
combine('format', kColorTextureFormats).
filter((p) => kTextureFormatInfo[p.format].color.storage).
beginSubcases().
combine('shaderScalarType', ['f32', 'u32', 'i32', 'bool', 'f16'])
).
beforeAllSubcases((t) => {
  if (t.params.shaderScalarType === 'f16') {
    t.selectDeviceOrSkipTestCase({ requiredFeatures: ['shader-f16'] });
  }

  if (!isTextureFormatUsableAsStorageFormat(t.params.format, t.isCompatibility)) {
    t.skip('storage usage is unsupported');
  }
}).
fn((t) => {
  const { format, shaderScalarType } = t.params;
  const info = kTextureFormatInfo[format];
  const validShaderScalarType = getPlainTypeInfo(info.color.type);
  const shaderValueType = `vec4<${shaderScalarType}>`;
  const wgsl = `
    @group(0) @binding(0) var tex: texture_storage_2d<${format}, read>;
    @compute @workgroup_size(1) fn main() {
        let v : ${shaderValueType} = textureLoad(tex, vec2u(0));
        _ = v;
    }
`;
  t.expectCompileResult(validShaderScalarType === shaderScalarType, wgsl);
});

g.test('texel_formats,as_value').
desc('Test that texel format cannot be used as value').
fn((t) => {
  const wgsl = `
    @compute @workgroup_size(1) fn main() {
        var i = rgba8unorm;
    }
`;
  t.expectCompileResult(false, wgsl);
});

const kValidTextureSampledTypes = ['f32', 'i32', 'u32'];

g.test('sampled_texture_types').
desc(
  `Test that for texture_xx<T>
- The sampled type T must be f32, i32, or u32
`
).
params((u) =>
u.
combine('textureType', ['texture_2d', 'texture_multisampled_2d']).
beginSubcases().
combine('sampledType', [
...kValidTextureSampledTypes,
'bool',
'vec2',
'mat2x2',
'1.0',
'1',
'1u']
)
).
fn((t) => {
  const { textureType, sampledType } = t.params;
  const wgsl = `@group(0) @binding(0) var tex: ${textureType}<${sampledType}>;`;
  t.expectCompileResult(kValidTextureSampledTypes.includes(sampledType), wgsl);
});

const kAccessModes = ['read', 'write', 'read_write'];

g.test('storage_texture_types').
desc(
  `Test that for texture_storage_xx<format, access>
- format must be an enumerant for one of the texel formats for storage textures
- access must be an enumerant for one of the access modes

Besides, the shader compilation should always pass regardless of whether the format supports the usage indicated by the access or not.
`
).
params((u) =>
u.combine('access', [...kAccessModes, 'storage']).combine('format', kAllTextureFormats)
).
fn((t) => {
  const { format, access } = t.params;
  const info = kTextureFormatInfo[format];
  // bgra8unorm is considered a valid storage format at shader compilation stage
  const isFormatValid =
  info.color?.storage ||
  info.depth?.storage ||
  info.stencil?.storage ||
  format === 'bgra8unorm';
  const isAccessValid = kAccessModes.includes(access);
  const wgsl = `@group(0) @binding(0) var tex: texture_storage_2d<${format}, ${access}>;`;
  t.expectCompileResult(isFormatValid && isAccessValid, wgsl);
});