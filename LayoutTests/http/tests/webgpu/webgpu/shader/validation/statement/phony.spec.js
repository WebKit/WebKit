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
        pass: true,
        usesF16: scalarTypeOf(Type[t]).kind === 'f16'
      }
    }),
    {}
  ),
  array: { value: 'array(1,2,3)', pass: true },
  struct: { value: 'S(1,2)', pass: true, gdecl: 'struct S{ a:u32, b:u32}' },
  atomic_u32: { value: 'xu', pass: false, gdecl: 'var<workgroup> xu: atomic<u32>;' },
  atomic_i32: { value: 'xi', pass: false, gdecl: 'var<workgroup> xi: atomic<i32>;' }
};

g.test('rhs_constructible').
desc(`Test that the rhs of 'phony assignment' can be a constructible.`).
params((u) => u.combine('type', keysOf(kConstructibleCases))).
beforeAllSubcases((t) => {
  const c = kConstructibleCases[t.params.type];
  if (c.usesF16) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const { value, pass, usesF16, gdecl } = kConstructibleCases[t.params.type];
  const code = `
${usesF16 ? 'enable f16;' : ''}
${gdecl ?? ''}
fn f() {
  _ = ${value};
}`;
  t.expectCompileResult(pass, code);
});

const kVarCases = {
  storage: { value: 'x', gdecl: '@group(0) @binding(0) var<storage> x: array<u32,1>;', pass: true },
  storage_unsized: {
    value: 'x',
    gdecl: '@group(0) @binding(0) var<storage> x: array<u32>;',
    pass: false
  },
  storage_atomic: {
    value: 'x',
    gdecl: '@group(0) @binding(0) var<storage,read_write> x: atomic<u32>;',
    pass: false
  },
  uniform: { value: 'x', gdecl: '@group(0) @binding(0) var<uniform> x: u32;', pass: true },
  texture: { value: 'x', gdecl: '@group(0) @binding(0) var x: texture_2d<f32>;', pass: true },
  sampler: { value: 'x', gdecl: '@group(0) @binding(0) var x: sampler;', pass: true },
  sampler_comparison: {
    value: 'x',
    gdecl: '@group(0) @binding(0) var x: sampler_comparison;',
    pass: true
  },
  private: { value: 'x', gdecl: 'var<private> x: u32;', pass: true },
  workgroup: { value: 'x', gdecl: 'var<workgroup> x: u32;', pass: true },
  workgroup_atomic: { value: 'x', gdecl: 'var<workgroup> x: atomic<u32>;', pass: false },
  override: { value: 'o', gdecl: 'override o: u32;', pass: true },
  function_var: { value: 'x', ldecl: 'var x: u32;', pass: true },
  let: { value: 'v', ldecl: 'let v = 1;', pass: true },
  const: { value: 'c', gdecl: 'const c = 1;', pass: true },
  function_const: { value: 'c', ldecl: 'const c = 1;', pass: true },
  ptr: { value: '&x', ldecl: 'var x: u32;', pass: true },
  ptr_to_unsized: {
    value: '&x',
    gdecl: '@group(0) @binding(0) var<storage> x: array<u32>;',
    pass: true
  },
  indexed: {
    value: 'x[0]',
    gdecl: '@group(0) @binding(0) var<storage> x: array<u32>;',
    pass: true
  },
  user_fn: { value: 'f', pass: false },
  builtin: { value: 'max', pass: false },
  builtin_call: { value: 'max(1,1)', pass: true },
  user_call: { value: 'callee()', pass: true, gdecl: 'fn callee() -> i32 { return 0; }' },
  undeclared: { value: 'does_not_exist', pass: false }
};

g.test('rhs_with_decl').
desc(`Test rhs of 'phony assignment' involving declared objects.`).
params((u) => u.combine('test', keysOf(kVarCases))).
fn((t) => {
  const { value, pass, gdecl, ldecl } = kVarCases[t.params.test];
  const code = `
${gdecl ?? ''}
@compute @workgroup_size(1)
fn f() {
  ${ldecl ?? ''}
  _ = ${value};
}`;
  t.expectCompileResult(pass, code);
});