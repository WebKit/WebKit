/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = 'Test pointer type validation';import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

g.test('address_space').
desc('Test address spaces in pointer type parameterization').
params((u) =>
u.combine('aspace', [
'function',
'private',
'workgroup',
'storage',
'uniform',
'handle',
'bad_aspace']
)
).
fn((t) => {
  const code = `alias T = ptr<${t.params.aspace}, u32>;`;
  const success = t.params.aspace !== 'handle' && t.params.aspace !== 'bad_aspace';
  t.expectCompileResult(success, code);
});

g.test('access_mode').
desc('Test access mode in pointer type parameterization').
params((u) =>
u.
combine('aspace', ['function', 'private', 'storage', 'uniform', 'workgroup']).
combine('access', ['read', 'write', 'read_write'])
).
fn((t) => {
  // Default access mode is tested above.
  const code = `alias T = ptr<${t.params.aspace}, u32, ${t.params.access}>;`;
  const success = t.params.aspace === 'storage' && t.params.access !== 'write';
  t.expectCompileResult(success, code);
});








const kTypeCases = {
  // Scalars
  bool: { type: `bool`, storable: true, aspace: 'function' },
  u32: { type: `u32`, storable: true },
  i32: { type: `i32`, storable: true },
  f32: { type: `f32`, storable: true },
  f16: { type: `f16`, storable: true, f16: true },

  // Vectors
  vec2u: { type: `vec2u`, storable: true },
  vec3i: { type: `vec3i`, storable: true },
  vec4f: { type: `vec4f`, storable: true },
  vec2_bool: { type: `vec2<bool>`, storable: true, aspace: 'workgroup' },
  vec3h: { type: `vec3h`, storable: true, f16: true },

  // Matrices
  mat2x2f: { type: `mat2x2f`, storable: true },
  mat3x4h: { type: `mat3x4h`, storable: true, f16: true },

  // Atomics
  atomic_u32: { type: `atomic<u32>`, storable: true },
  atomic_i32: { type: `atomic<i32>`, storable: true },

  // Arrays
  array_sized_u32: { type: `array<u32, 4>`, storable: true },
  array_sized_vec4f: { type: `array<vec4f, 16>`, storable: true },
  array_sized_S: { type: `array<S, 2>`, storable: true },
  array_runtime_u32: { type: `array<u32>`, storable: true },
  array_runtime_S: { type: `array<S>`, storable: true },
  array_runtime_atomic_u32: { type: `array<atomic<u32>>`, storable: true },
  array_override_u32: { type: `array<u32, o>`, storable: true, aspace: 'workgroup' },

  // Structs
  struct_S: { type: `S`, storable: true },
  struct_T: { type: `T`, storable: true },

  // Pointers
  ptr_function_u32: { type: `ptr<function, u32>`, storable: false },
  ptr_workgroup_bool: { type: `ptr<workgroup, bool>`, storable: false },

  // Sampler (while storable, can only be in the handle address space)
  sampler: { type: `sampler`, storable: false },

  // Texture (while storable, can only be in the handle address space)
  texture_2d: { type: `texture_2d<f32>`, storable: false },

  // Alias
  alias: { type: `u32_alias`, storable: true },

  // Reference
  reference: { type: `ref<function, u32>`, storable: false, aspace: 'function' }
};

g.test('type').
desc('Tests that pointee type must be storable').
params((u) => u.combine('case', keysOf(kTypeCases))).
beforeAllSubcases((t) => {
  if (kTypeCases[t.params.case].f16) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const testcase = kTypeCases[t.params.case];
  const aspace = testcase.aspace ?? 'storage';
  const access = testcase.type.includes('atomic') ? ', read_write' : '';
  const code = `${testcase.f16 ? 'enable f16;' : ''}
    override o : u32;
    struct S { x : u32 }
    struct T { s : array<S> }
    alias u32_alias = u32;
    alias Type = ptr<${aspace}, ${testcase.type}${access}>;`;
  t.expectCompileResult(testcase.storable, code);
});