/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for atomicStoreMin and atomicStoreMax builtins.
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf } from '../../../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kAtomicOps = {
  atomicStoreMin: (a, v) => `atomicStoreMin(${a}, ${v})`,
  atomicStoreMax: (a, v) => `atomicStoreMax(${a}, ${v})`
};

g.test('requires_extension').
desc('Tests that atomicStoreMin and atomicStoreMax require the atomic_vec2u_min_max extension').
params((u) => u.combine('op', keysOf(kAtomicOps)).combine('enable', [true, false])).
fn((t) => {
  const code = `
${t.params.enable ? 'enable atomic_vec2u_min_max;' : ''}
@group(0) @binding(0) var<storage, read_write> a : atomic<vec2u>;
fn foo() {
  ${kAtomicOps[t.params.op]('&a', 'vec2u(1, 1)')};
}
`;
  t.expectCompileResult(t.params.enable, code);
});

g.test('stage').
desc('Tests that atomicStoreMin and atomicStoreMax must not be used in vertex shader stage').
params((u) =>
u.combine('op', keysOf(kAtomicOps)).combine('stage', ['fragment', 'vertex', 'compute'])
).
fn((t) => {
  let code = `
enable atomic_vec2u_min_max;
@group(0) @binding(0) var<storage, read_write> a : atomic<vec2u>;
`;

  const op = kAtomicOps[t.params.op]('&a', 'vec2u(1, 1)');

  switch (t.params.stage) {
    case 'compute':
      code += `
@compute @workgroup_size(1,1,1) fn main() {
  ${op};
}`;
      break;

    case 'fragment':
      code += `
@fragment fn main() -> @location(0) vec4<f32> {
  ${op};
  return vec4<f32>();
}`;
      break;

    case 'vertex':
      code += `
@vertex fn vmain() -> @builtin(position) vec4<f32> {
  ${op};
  return vec4<f32>();
}`;
      break;
  }

  const pass = t.params.stage !== 'vertex';
  t.expectCompileResult(pass, code);
});

g.test('atomic_ptr_parameterization').
desc('Tests the valid atomic_ptr parameters').
params((u) =>
u.
combine('op', keysOf(kAtomicOps)).
beginSubcases().
combine('aspace', ['storage', 'workgroup', 'private', 'uniform', 'function']).
combine('access', ['read', 'read_write']).
combine('style', ['param', 'var']).
filter((t) => {
  switch (t.aspace) {
    case 'uniform':
      return t.style === 'param' && t.access === 'read';
    case 'workgroup':
      return t.access === 'read_write';
    case 'function':
    case 'private':
      return t.style === 'param' && t.access === 'read_write';
    default:
      return true;
  }
})
).
fn((t) => {
  let moduleVar = ``;
  let functionVar = ``;
  let param = ``;
  let aParam = ``;
  if (t.params.style === 'var') {
    aParam = `&a`;
    switch (t.params.aspace) {
      case 'storage':
        moduleVar = `@group(0) @binding(0) var<storage, ${t.params.access}> a : atomic<vec2u>;`;
        break;
      case 'workgroup':
        moduleVar = `var<workgroup> a : atomic<vec2u>;`;
        break;
      case 'uniform':
        moduleVar = `@group(0) @binding(0) var<uniform> a : atomic<vec2u>;`;
        break;
      case 'private':
        moduleVar = `var<private> a : atomic<vec2u>;`;
        break;
      case 'function':
        functionVar = `var a : atomic<vec2u>;`;
        break;
      default:
        break;
    }
  } else {
    const aspaceParam = t.params.aspace === 'storage' ? `, ${t.params.access}` : ``;
    param = `p : ptr<${t.params.aspace}, atomic<vec2u>${aspaceParam}>`;
    aParam = `p`;
  }

  const code = `
enable atomic_vec2u_min_max;
${moduleVar}
fn foo(${param}) {
  ${functionVar}
  ${kAtomicOps[t.params.op](aParam, 'vec2u(1, 1)')};
}
`;

  const aspaceOK = t.params.aspace === 'storage';
  const accessOK = t.params.access === 'read_write';
  t.expectCompileResult(aspaceOK && accessOK, code);
});

g.test('value_parameter').
desc('Validates that the value parameter must be vec2u').
params((u) =>
u.
combine('op', keysOf(kAtomicOps)).
beginSubcases().
combine('valueType', [
'vec2u',
'vec2<u32>',
'vec2i',
'vec2<i32>',
'vec2f',
'vec2<f32>',
'u32',
'i32',
'vec2<AbstractInt>',
'vec2<AbstractFloat>']
)
).
fn((t) => {
  let value = '';
  switch (t.params.valueType) {
    case 'vec2u':
    case 'vec2<u32>':
      value = 'vec2u(1u, 1u)';
      break;
    case 'vec2i':
    case 'vec2<i32>':
      value = 'vec2i(1i, 1i)';
      break;
    case 'vec2f':
    case 'vec2<f32>':
      value = 'vec2f(1f, 1f)';
      break;
    case 'u32':
      value = '1u';
      break;
    case 'i32':
      value = '1i';
      break;
    case 'vec2<AbstractInt>':
      value = 'vec2(1, 1)';
      break;
    case 'vec2<AbstractFloat>':
      value = 'vec2(1.0, 1.0)';
      break;
  }

  const code = `
enable atomic_vec2u_min_max;
@group(0) @binding(0) var<storage, read_write> a : atomic<vec2u>;
fn foo() {
  ${kAtomicOps[t.params.op]('&a', value)};
}
`;

  const expect =
  t.params.valueType === 'vec2u' ||
  t.params.valueType === 'vec2<u32>' ||
  t.params.valueType === 'vec2<AbstractInt>';
  t.expectCompileResult(expect, code);
});

g.test('return_type').
desc('Validates that atomicStoreMin and atomicStoreMax return void').
params((u) => u.combine('op', keysOf(kAtomicOps))).
fn((t) => {
  const code = `
enable atomic_vec2u_min_max;
@group(0) @binding(0) var<storage, read_write> a: atomic<vec2u>;
fn foo() {
  let x = ${kAtomicOps[t.params.op]('&a', 'vec2u(1, 1)')};
}
`;

  t.expectCompileResult(false, code);
});

g.test('atomic_ptr_type').
desc('Validates that the atomic_ptr parameter must be atomic<vec2u>').
params((u) =>
u.
combine('op', keysOf(kAtomicOps)).
beginSubcases().
combine('atomicType', ['vec4u', 'vec2u', 'u32', 'i32'])
).
fn((t) => {
  const code = `
enable atomic_vec2u_min_max;
@group(0) @binding(0) var<storage, read_write> a : atomic<${t.params.atomicType}>;
fn foo() {
  ${kAtomicOps[t.params.op]('&a', `${t.params.atomicType} ( 1) `)};
}
`;

  const expect = t.params.atomicType === 'vec2u';
  t.expectCompileResult(expect, code);
});

g.test('non_atomic').
desc('Test that non-atomic vec2u are rejected').
params((u) =>
u.
combine('op', keysOf(kAtomicOps)).
combine('addrspace', ['storage', 'workgroup']).
combine('atomic', [true, false])
).
fn((t) => {
  const type = t.params.atomic ? 'atomic<vec2u>' : 'vec2u';
  let decl = '';
  if (t.params.addrspace === 'storage') {
    decl = `@group(0) @binding(0) var<storage, read_write> a : ${type}`;
  } else if (t.params.addrspace === 'workgroup') {
    decl = `var<workgroup> a : ${type}`;
  }

  const code = `
enable atomic_vec2u_min_max;
${decl};
fn foo() {
  ${kAtomicOps[t.params.op]('&a', 'vec2u(1, 1)')};
}
`;

  t.expectCompileResult(t.params.atomic && t.params.addrspace === 'storage', code);
});