/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `Validation for phony assignment statements`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { scalarTypeOf, Type } from '../../../util/conversion.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);








const kConstructibleTypes = [
'bool',
'i32',
'u32',
'f32',
'f16',
'vec2f',
'vec3h',
'vec4u',
'vec3b',
'mat2x3f',
'mat4x2h',
'abstractInt',
'abstractFloat'];


const kConstructibleCases = {
  ...kConstructibleTypes.reduce(
    (acc, t) => ({
      ...acc,
      [t]: {
        value: Type[t].create(1).wgsl(),
        pass: t === 'i32' || t === 'u32' || t === 'abstractInt',
        usesF16: scalarTypeOf(Type[t]).kind === 'f16'
      }
    }),
    {}
  ),
  array: { value: 'array(1,2,3)', pass: false },
  struct: { value: 'S(1,2)', pass: false, gdecl: 'struct S{ a:u32, b:u32}' },
  atomic_u32: { value: 'xu', pass: false, gdecl: 'var<workgroup> xu: atomic<u32>;' },
  atomic_i32: { value: 'xi', pass: false, gdecl: 'var<workgroup> xi: atomic<i32>;' }
};

g.test('var_init_type').
desc(`Test increment and decrement of a variable of various types`).
params((u) => u.combine('type', keysOf(kConstructibleCases)).combine('direction', ['up', 'down'])).
beforeAllSubcases((t) => {
  const c = kConstructibleCases[t.params.type];
  if (c.usesF16) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const { value, pass, usesF16, gdecl } = kConstructibleCases[t.params.type];
  const operator = t.params.direction === 'up' ? '++' : '--';
  const code = `
${usesF16 ? 'enable f16;' : ''}
${gdecl ?? ''}
fn f() {
  var a = ${value};
  a${operator};
}`;
  t.expectCompileResult(pass, code);
});









const kComponentCases = {
  v2u_x: { type: 'vec2u', wgsl: 'a.x', pass: true },
  v2u_y: { type: 'vec2u', wgsl: 'a.y', pass: true },
  v3u_x: { type: 'vec3u', wgsl: 'a.x', pass: true },
  v3u_y: { type: 'vec3u', wgsl: 'a.y', pass: true },
  v3u_z: { type: 'vec3u', wgsl: 'a.z', pass: true },
  v4u_x: { type: 'vec4u', wgsl: 'a.x', pass: true },
  v4u_y: { type: 'vec4u', wgsl: 'a.y', pass: true },
  v4u_z: { type: 'vec4u', wgsl: 'a.z', pass: true },
  v4u_w: { type: 'vec4u', wgsl: 'a.w', pass: true },
  v2i_x: { type: 'vec2i', wgsl: 'a.x', pass: true },
  v2i_y: { type: 'vec2i', wgsl: 'a.y', pass: true },
  v3i_x: { type: 'vec3i', wgsl: 'a.x', pass: true },
  v3i_y: { type: 'vec3i', wgsl: 'a.y', pass: true },
  v3i_z: { type: 'vec3i', wgsl: 'a.z', pass: true },
  v4i_x: { type: 'vec4i', wgsl: 'a.x', pass: true },
  v4i_y: { type: 'vec4i', wgsl: 'a.y', pass: true },
  v4i_z: { type: 'vec4i', wgsl: 'a.z', pass: true },
  v4i_w: { type: 'vec4i', wgsl: 'a.w', pass: true },
  v2u_xx: { type: 'vec2u', wgsl: 'a.xx', pass: false },
  v2u_indexed: { type: 'vec2u', wgsl: 'a[0]', pass: true },
  v2f_x: { type: 'vec2f', wgsl: 'a.x', pass: false },
  v2h_x: { type: 'vec2h', wgsl: 'a.x', pass: false, usesF16: true },
  mat2x2f: { type: 'mat2x2f', wgsl: 'a[0][0]', pass: false },
  mat2x2h: { type: 'mat2x2h', wgsl: 'a[0][0]', pass: false, usesF16: true },
  array: { type: 'array<i32,2>', wgsl: 'a', pass: false },
  array_i: { type: 'array<i32,2>', wgsl: 'a[0]', pass: true },
  array_f: { type: 'array<f32,2>', wgsl: 'a[0]', pass: false },
  struct: { type: 'S', wgsl: 'S', pass: false, gdecl: 'struct S{field:i32}' },
  struct_var: { type: 'S', wgsl: 'a', pass: false, gdecl: 'struct S{field:i32}' },
  struct_field: { type: 'S', wgsl: 'a.field', pass: true, gdecl: 'struct S{field:i32}' }
};

g.test('component').
desc('Test increment and decrement of component of various types').
params((u) => u.combine('type', keysOf(kComponentCases)).combine('direction', ['up', 'down'])).
beforeAllSubcases((t) => {
  const c = kComponentCases[t.params.type];
  if (c.usesF16) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const { type, wgsl, pass, usesF16, gdecl } = kComponentCases[t.params.type];
  const operator = t.params.direction === 'up' ? '++' : '--';
  const code = `
${usesF16 ? 'enable f16;' : ''}
${gdecl ?? ''}
fn f() {
  var a: ${type};
  ${wgsl}${operator};
}`;
  t.expectCompileResult(pass, code);
});