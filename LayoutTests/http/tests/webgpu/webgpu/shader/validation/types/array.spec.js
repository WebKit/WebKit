/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for array types
`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kValidCases = {
  // Basic element types.
  i32: `alias T = array<i32>;`,
  u32: `alias T = array<u32>;`,
  f32: `alias T = array<f32>;`,
  f16: `enable f16;\nalias T = array<f16>;`,
  bool: `alias T = array<bool>;`,

  // Composite elements
  vec2u: `alias T = array<vec2u>;`,
  vec3i: `alias T = array<vec3i>;`,
  vec4f: `alias T = array<vec4f>;`,
  array: `alias T = array<array<u32, 4>>;`,
  struct: `struct S { x : u32 }\nalias T = array<S>;`,
  mat2x2f: `alias T = array<mat2x2f>;`,
  mat4x4h: `enable f16;\nalias T = array<mat4x4h>;`,

  // Atomic elements
  atomicu: `alias T = array<atomic<u32>>;`,
  atomici: `alias T = array<atomic<i32>>;`,

  // Count expressions
  literal_count: `alias T = array<u32, 4>;`,
  literali_count: `alias T = array<u32, 4i>;`,
  literalu_count: `alias T = array<u32, 4u>;`,
  const_count: `const x = 8;\nalias T = array<u32, x>;`,
  const_expr_count1: `alias T = array<u32, 1 + 3>;`,
  const_expr_count2: `const x = 4;\nalias T = array<u32, x * 2>;`,
  override_count: `override x : u32;\nalias T = array<u32, x>;`,
  override_expr1: `override x = 2;\nalias T = array<u32, vec2(x,x).x>;`,
  override_expr2: `override x = 1;\nalias T = array<u32, x + 1>;`,
  override_zero: `override x = 0;\nalias T = array<u32, x>;`,
  override_neg: `override x = -1;\nalias T = array<u32, x>;`,

  // Same array types
  same_const_value1: `
    const x = 8;
    const y = 8;
    var<private> v : array<u32, x> = array<u32, y>();`,
  same_const_value2: `
    const x = 8;
    var<private> v : array<u32, x> = array<u32, 8>();`,
  same_const_value3: `
    var<private> v : array<u32, 8i> = array<u32, 8u>();`,
  same_override: `
    requires unrestricted_pointer_parameters;
    override x : u32;
    var<workgroup> v : array<u32, x>;
    fn bar(p : ptr<workgroup, array<u32, x>>) { }
    fn foo() { bar(&v); }`,

  // Shadow
  shadow: `alias array = vec2f;`
};

g.test('valid').
desc('Valid array type tests').
params((u) => u.combine('case', keysOf(kValidCases))).
beforeAllSubcases((t) => {
  const code = kValidCases[t.params.case];
  if (code.indexOf('f16') >= 0) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const code = kValidCases[t.params.case];
  t.skipIf(
    code.indexOf('unrestricted') >= 0 && !t.hasLanguageFeature('unrestricted_pointer_parameters'),
    'Test requires unrestricted_pointer_parameters'
  );
  t.expectCompileResult(true, code);
});

const kInvalidCases = {
  f16_without_enable: `alias T = array<f16>;`,
  runtime_nested: `alias T = array<array<u32>, 4>;`,
  override_nested: `
    override x : u32;
    alias T = array<array<u32, x>, 4>;`,
  override_nested_struct: `
    override x : u32;
    struct T { x : array<u32, x> }`,
  zero_size: `alias T = array<u32, 0>;`,
  negative_size: `alias T = array<u32, 1 - 2>;`,
  const_zero: `const x = 0;\nalias T = array<u32, x>;`,
  const_neg: `const x = 1;\nconst y = 2;\nalias T = array<u32, x - y>;`,
  incompatible_overrides: `
    requires unrestricted_pointer_parameters;
    override x = 8;
    override y = 8;
    var<workgroup> v : array<u32, x>
    fn bar(p : ptr<workgroup, array<u32 y>>) { }
    fn foo() { bar(&v); }`
};

g.test('invalid').
desc('Invalid array type tests').
params((u) => u.combine('case', keysOf(kInvalidCases))).
beforeAllSubcases((t) => {
  const code = kInvalidCases[t.params.case];
  if (code.indexOf('f16') >= 0) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const code = kInvalidCases[t.params.case];
  t.skipIf(
    code.indexOf('unrestricted') >= 0 && !t.hasLanguageFeature('unrestricted_pointer_parameters'),
    'Test requires unrestricted_pointer_parameters'
  );
  t.expectCompileResult(false, code);
});