/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for the WGSL immediate address space.
`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

import { skipIfImmediateDataNotSupported } from './util.js';

export const g = makeTestGroup(ShaderValidationTest);

const kImmediateFeature = 'immediate_address_space';
const kImmediateHeader = `requires ${kImmediateFeature};`;

const kValidStoreTypes = {
  u32: { enable: ``, prelude: ``, type: `u32` },
  i32: { enable: ``, prelude: ``, type: `i32` },
  f32: { enable: ``, prelude: ``, type: `f32` },
  f16: { enable: `enable f16;`, prelude: ``, type: `f16` },
  vec2u: { enable: ``, prelude: ``, type: `vec2u` },
  vec3i: { enable: ``, prelude: ``, type: `vec3i` },
  vec4f: { enable: ``, prelude: ``, type: `vec4f` },
  vec3h: { enable: `enable f16;`, prelude: ``, type: `vec3h` },
  mat2x2f: { enable: ``, prelude: ``, type: `mat2x2f` },
  struct_numeric: { enable: ``, prelude: `struct S { a : u32, b : vec4f }`, type: `S` }
};

const kInvalidStoreTypes = {
  bool: { enable: ``, prelude: ``, type: `bool` },
  vec2_bool: { enable: ``, prelude: ``, type: `vec2<bool>` },
  atomic_u32: { enable: ``, prelude: ``, type: `atomic<u32>` },
  ptr_function_u32: { enable: ``, prelude: ``, type: `ptr<function, u32>` },
  sampler: { enable: ``, prelude: ``, type: `sampler` },
  sampler_comparison: { enable: ``, prelude: ``, type: `sampler_comparison` },
  texture_2d: { enable: ``, prelude: ``, type: `texture_2d<f32>` },
  runtime_array: { enable: ``, prelude: ``, type: `array<u32>` },
  fixed_array: { enable: ``, prelude: ``, type: `array<u32, 4>` },
  struct_runtime_array: { enable: ``, prelude: `struct S { data : array<u32> }`, type: `S` },
  struct_fixed_array: {
    enable: ``,
    prelude: `struct S { data : array<vec4u, 4> }`,
    type: `S`
  }
};

g.test('store_type,valid').
desc('Validates immediate store types supported by the current WGSL immediate implementation.').
params((u) => u.combine('type', keysOf(kValidStoreTypes))).
fn((t) => {
  skipIfImmediateDataNotSupported(t);
  const testcase = kValidStoreTypes[t.params.type];
  if (testcase.enable.includes('f16')) {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }
  const wgsl = `
${kImmediateHeader}
${testcase.enable}
${testcase.prelude}
var<immediate> data : ${testcase.type};
@compute @workgroup_size(1)
fn main() {
  _ = data;
}`;
  t.expectCompileResult(true, wgsl);
});

g.test('store_type,invalid').
desc('Validates types that cannot be used for immediate variables.').
params((u) => u.combine('type', keysOf(kInvalidStoreTypes))).
fn((t) => {
  skipIfImmediateDataNotSupported(t);
  const testcase = kInvalidStoreTypes[t.params.type];
  const wgsl = `
${kImmediateHeader}
${testcase.enable}
${testcase.prelude}
var<immediate> data : ${testcase.type};
@compute @workgroup_size(1)
fn main() {
  _ = data;
}`;
  t.expectCompileResult(false, wgsl);
});

g.test('scope').
desc('Validates that immediate variables cannot be declared at function scope.').
params((u) => u.combine('addressSpace', ['function', 'immediate'])).
fn((t) => {
  skipIfImmediateDataNotSupported(t);
  const wgsl = `
${kImmediateHeader}
@compute @workgroup_size(1)
fn main() {
  var<${t.params.addressSpace}> data : u32;
  _ = data;
}`;
  t.expectCompileResult(t.params.addressSpace === 'function', wgsl);
});

g.test('binding_attributes').
desc('Validates that @group and @binding are not allowed on immediate variables.').
params((u) =>
u.combine('group', ['', '@group(0)']).combine('binding', ['', '@binding(0)'])
).
fn((t) => {
  skipIfImmediateDataNotSupported(t);
  const wgsl = `
${kImmediateHeader}
${t.params.group} ${t.params.binding} var<immediate> data : u32;
@compute @workgroup_size(1)
fn main() {
  _ = data;
}`;
  t.expectCompileResult(t.params.group === '' && t.params.binding === '', wgsl);
});

g.test('access_mode').
desc('Validates that immediate variables cannot spell an access mode.').
params((u) => u.combine('accessMode', ['', 'read', 'write', 'read_write'])).
fn((t) => {
  skipIfImmediateDataNotSupported(t);
  const suffix = t.params.accessMode === '' ? '' : `, ${t.params.accessMode}`;
  const wgsl = `
${kImmediateHeader}
var<immediate${suffix}> data : u32;
@compute @workgroup_size(1)
fn main() {
  _ = data;
}`;
  t.expectCompileResult(t.params.accessMode === '', wgsl);
});

const kEntryPointCases = {
  one_used: {
    valid: true,
    body: `
var<immediate> a : u32;
@compute @workgroup_size(1)
fn main() {
  _ = a;
}`
  },
  two_declared_one_used: {
    valid: true,
    body: `
var<immediate> a : u32;
var<immediate> b : u32;
@compute @workgroup_size(1)
fn main() {
  _ = a;
}`
  },
  two_entry_points_one_each: {
    valid: true,
    body: `
var<immediate> a : u32;
var<immediate> b : u32;
@compute @workgroup_size(1)
fn main_a() {
  _ = a;
}
@compute @workgroup_size(1)
fn main_b() {
  _ = b;
}`
  },
  one_entry_point_uses_two_directly: {
    valid: false,
    body: `
var<immediate> a : u32;
var<immediate> b : u32;
@compute @workgroup_size(1)
fn main() {
  _ = a + b;
}`
  },
  one_entry_point_uses_two_through_helper: {
    valid: false,
    body: `
var<immediate> a : u32;
var<immediate> b : u32;
fn read_b() -> u32 {
  return b;
}
@compute @workgroup_size(1)
fn main() {
  _ = a + read_b();
}`
  }
};

g.test('entry_point_interface').
desc('Validates one statically used immediate variable per entry point.').
params((u) => u.combine('case', keysOf(kEntryPointCases))).
fn((t) => {
  skipIfImmediateDataNotSupported(t);
  const testcase = kEntryPointCases[t.params.case];
  t.expectCompileResult(testcase.valid, `${kImmediateHeader}\n${testcase.body}`);
});

const kPointerCases = {
  alias_module_scope: {
    valid: true,
    needsUnrestrictedPointerParameters: false,
    body: `
alias P = ptr<immediate, u32>;
var<immediate> data : u32;
@compute @workgroup_size(1)
fn main() {
  let p : P = &data;
  _ = *p;
}`
  },
  let_inside_function: {
    valid: true,
    needsUnrestrictedPointerParameters: false,
    body: `
var<immediate> data : u32;
@compute @workgroup_size(1)
fn main() {
  let p : ptr<immediate, u32> = &data;
  _ = *p;
}`
  },
  write_through_pointer: {
    valid: false,
    needsUnrestrictedPointerParameters: false,
    body: `
var<immediate> data : u32;
@compute @workgroup_size(1)
fn main() {
  let p : ptr<immediate, u32> = &data;
  *p = 1u;
}`
  },
  pointer_parameter: {
    valid: true,
    needsUnrestrictedPointerParameters: true,
    body: `
var<immediate> data : u32;
fn read_data(p : ptr<immediate, u32>) -> u32 {
  return *p;
}
@compute @workgroup_size(1)
fn main() {
  _ = read_data(&data);
}`
  },
  explicit_read_access: {
    valid: false,
    needsUnrestrictedPointerParameters: false,
    body: `
alias P = ptr<immediate, u32, read>;`
  },
  explicit_write_access: {
    valid: false,
    needsUnrestrictedPointerParameters: false,
    body: `
alias P = ptr<immediate, u32, write>;`
  },
  explicit_read_write_access: {
    valid: false,
    needsUnrestrictedPointerParameters: false,
    body: `
alias P = ptr<immediate, u32, read_write>;`
  },
  missing_store_type: {
    valid: false,
    needsUnrestrictedPointerParameters: false,
    body: `
alias P = ptr<immediate>;`
  }
};

g.test('pointers').
desc('Validates ptr<immediate> type creation, use, access modes, and function parameters.').
params((u) => u.combine('case', keysOf(kPointerCases))).
fn((t) => {
  skipIfImmediateDataNotSupported(t);
  const testcase = kPointerCases[t.params.case];
  const unrestrictedHeader =
  testcase.needsUnrestrictedPointerParameters &&
  t.hasLanguageFeature('unrestricted_pointer_parameters') ?
  'requires unrestricted_pointer_parameters;\n' :
  '';
  const expected =
  testcase.valid && (
  !testcase.needsUnrestrictedPointerParameters ||
  t.hasLanguageFeature('unrestricted_pointer_parameters'));
  const wgsl = `${kImmediateHeader}\n${unrestrictedHeader}${testcase.body}`;
  t.expectCompileResult(expected, wgsl);
});