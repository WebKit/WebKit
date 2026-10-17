/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for bufferView
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf, objectsToRecord } from '../../../../../../common/util/data_tables.js';

import { Type, elementTypeOf, kAllScalarsAndVectors } from '../../../../../util/conversion.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

g.test('must_use').
desc('Tests that the builtin has the @must_use attribute').
params((u) => u.combine('must_use', [true, false])).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const wgsl = `
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  ${t.params.must_use ? 'let p = ' : ''}bufferView<u32>(&v, 0);
}`;

  t.expectCompileResult(t.params.must_use, wgsl);
});

const kTypes = objectsToRecord(kAllScalarsAndVectors);

g.test('return_store_type').
desc('Validates return type').
params((u) =>
u.
combine('template_type', keysOf(kTypes)).
filter((t) => {
  const type = kTypes[t.template_type];
  const eleType = elementTypeOf(type);
  return (
    eleType !== Type.abstractInt && eleType !== Type.abstractFloat && eleType !== Type.bool);

}).
combine('return_type', keysOf(kTypes)).
filter((t) => {
  const type = kTypes[t.return_type];
  const eleType = elementTypeOf(type);
  return (
    eleType !== Type.abstractInt && eleType !== Type.abstractFloat && eleType !== Type.bool);

}).
beginSubcases().
combine('ptr', [false, true])
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const template_type = kTypes[t.params.template_type];
  const return_type = kTypes[t.params.return_type];
  let ret_type = return_type.toString();
  if (t.params.ptr) {
    ret_type = `ptr<storage, ${ret_type}>`;
  }
  let enables = ``;
  if (template_type.requiresF16() || return_type.requiresF16()) {
    enables = `enable f16;`;
  }
  const wgsl = `
${enables}
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let res : ${ret_type} = bufferView<${template_type.toString()}>(&v, 0);
}`;

  t.expectCompileResult(t.params.ptr && template_type === return_type, wgsl);
});

g.test('buffer_type').
desc('Validates the buffer parameter type').
params((u) =>
u.
combine('type', [
'unsized_ro_storage',
'unsized_storage',
'sized_ro_storage',
'sized_storage',
'sized_uniform',
'sized_workgroup',
'override_workgroup']
).
beginSubcases().
combine('ptr', [false, true])
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const aspace = t.params.type.substr(t.params.type.lastIndexOf('_') + 1);
  let access = '';
  switch (t.params.type) {
    case 'unsized_ro_storage':
    case 'sized_ro_storage':
      access = ', read';
      break;
    case 'unsized_storage':
    case 'sized_storage':
      access = ', read_write';
      break;
    default:
      break;
  }
  const wgsl = `
@group(0) @binding(0) var<storage> unsized_ro_storage : buffer;
@group(0) @binding(1) var<storage, read_write> unsized_storage : buffer;
@group(0) @binding(2) var<storage> sized_ro_storage : buffer<128>;
@group(0) @binding(3) var<storage, read_write> sized_storage : buffer<128>;
@group(0) @binding(4) var<uniform> sized_uniform : buffer<128>;
var<workgroup> sized_workgroup : buffer<128>;
override o : u32;
var<workgroup> override_workgroup : buffer<o>;

@compute @workgroup_size(1)
fn main() {
  let p : ptr<${aspace}, u32${access}> = bufferView<u32>(${t.params.ptr ? '&' : ''}${
  t.params.type
  }, 0);
}`;

  t.expectCompileResult(t.params.ptr, wgsl);
});

g.test('offset_type').
desc('Validates the offset parameter type').
params((u) => u.combine('type', keysOf(kTypes))).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const type = kTypes[t.params.type];
  const enables = type.requiresF16() ? 'enable f16;' : '';
  const wgsl = `
${enables}
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<u32>(&v, ${type.create(0).wgsl()});
}`;

  t.expectCompileResult(
    type === Type.abstractInt || type === Type.u32 || type === Type.i32,
    wgsl
  );
});








const kEarlyEvalCases = {
  ro_storage_buffer: {
    code: `
@group(0) @binding(0) var<storage> v : buffer<16>;
fn foo() {
  let p = bufferView<vec4u>(&v, 0);
}`,
    valid: true
  },
  storage_buffer: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer<16>;
fn foo() {
  let p = bufferView<vec4u>(&v, 0);
}`,
    valid: true
  },
  uniform_buffer: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<16>;
fn foo() {
  let p = bufferView<vec4u>(&v, 0);
}`,
    valid: true
  },
  workgroup_buffer: {
    code: `
var<workgroup> v : buffer<16>;
fn foo() {
  let p = bufferView<vec4u>(&v, 0);
}`,
    valid: true
  },
  ro_storage_buffer_too_small_for_type: {
    code: `
@group(0) @binding(0) var<storage> v : buffer<8>;
fn foo() {
  let p = bufferView<vec4u>(&v, 0);
}`,
    valid: false
  },
  uniform_buffer_too_small_with_const_offset: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<24>;
fn foo() {
  let p = bufferView<vec4u>(&v, 16);
}`,
    valid: false
  },
  uniform_buffer_too_small_with_override_offset: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<24>;
override offset : u32;
fn foo() {
  let p = bufferView<vec4u>(&v, offset);
}`,
    valid: 'pipeline',
    constants: { offset: 16 }
  },
  workgroup_buffer_too_small_with_override: {
    code: `
override size : u32;
var<workgroup> v : buffer<size>;
fn foo() {
  let p = bufferView<vec4u>(&v, 0);
}`,
    valid: 'pipeline',
    constants: { size: 12 }
  },
  storage_buffer_too_small_for_one_array_element: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer<28>;
struct S {
  a: vec4u,
  b: vec4f,
}
fn foo() {
  let p = bufferView<array<S>>(&v, 0);
}`,
    valid: false
  },
  storage_buffer_out_of_range_u32: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
struct S {
  a: vec4u,
  b: vec4f,
}
fn foo() {
  let p = buffer<S>(&v, 4294967279);
}`,
    valid: false
  },
  uniform_buffer_too_small_through_unsized_function: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<28>;
fn bar(p : ptr<uniform, buffer>) {
  let q = bufferView<vec4u>(p, 16);
}
fn foo() {
  bar(&v);
}`,
    valid: false,
    ptr_param: true
  },
  uniform_buffer_too_small_through_unsized_function_override: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<28>;
override offset : u32;
fn bar(p : ptr<uniform, buffer>) {
  let q = bufferView<vec4u>(p, offset);
}
fn foo() {
  bar(&v);
}`,
    valid: 'pipeline',
    constants: { offset: 16 },
    ptr_param: true
  },
  workgroup_to_smaller_size_param: {
    code: `
var<workgroup> v : buffer<128>;
fn bar(p : ptr<workgroup, buffer<16>) {
  let q = bufferView<array<vec2u, 3>(p, 0);
}
fn foo() {
  bar(&v);
}`,
    valid: false,
    ptr_param: true
  },
  ro_storage_unsized_buffer_to_sized_param: {
    code: `
@group(0) @binding(0) var<storage> v : buffer;
fn bar(p : ptr<storage, buffer<16>) {
  let q = bufferView<array<vec2u, 3>(p, 0);
}
fn foo() {
  bar(&v);
}`,
    valid: false,
    ptr_param: true
  },
  offset_not_aligned: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<128>;
fn foo() {
  let p = bufferView<vec4u>(&v, 12);
}`,
    valid: false
  },
  offset_not_aligned_override: {
    code: `
override offset : u32;
@group(0) @binding(0) var<uniform> v : buffer<128>;
fn foo() {
  let p = bufferView<vec4u>(&v, offset);
}`,
    valid: 'pipeline',
    constants: { offset: 12 }
  },
  offset_negative: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<128>;
fn foo() {
  let p = bufferView<vec4u>(&v, -16);
}`,
    valid: false
  },
  offset_negative_override: {
    code: `
override offset : u32;
@group(0) @binding(0) var<uniform> v : buffer<128>;
fn foo() {
  let p = bufferView<vec4u>(&v, offset);
}`,
    valid: 'pipeline',
    constants: { offset: -16 }
  }
};

g.test('early_eval_errors').
desc('Test shader-creation and pipeline-creation errors').
params((u) => u.combine('case', keysOf(kEarlyEvalCases))).
fn((t) => {
  const testcase = kEarlyEvalCases[t.params.case];
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  if (testcase.ptr_param === true) {
    t.skipIfLanguageFeatureNotSupported('unrestricted_pointer_parameters');
  }

  t.expectCompileResult(testcase.valid !== false, testcase.code);
  if (testcase.valid !== false) {
    t.expectPipelineResult({
      expectedResult: testcase.valid === true,
      code: testcase.code,
      constants: testcase.constants,
      statements: ['foo();']
    });
  }
});






const kLHSCallCases = {
  u32: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  *bufferView<u32>(&v, 0) = 123u;
}`
  },
  u32_array: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  (*bufferView<array<u32, 4>>(&v, 0))[0] = 123u;
}`
  },
  f32_vector_letter: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  (*bufferView<vec4f>(&v, 0)).x = 13.3f;
}`
  },
  f32_struct_access: {
    code: `
struct S {
  member_a: f32,
}
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  (*bufferView<S>(&v, 0)).member_a = 13.3f;
}`
  },
  u32_array_ptr_composite: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  bufferView<array<u32, 4>>(&v, 0)[0] = 123u;
}`,
    requires: ['pointer_composite_access']
  },
  vec4u_swizzle_assign: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  (*bufferView<vec4u>(&v, 0)).zxy = vec3u(42);
}`,
    requires: ['swizzle_assignment']
  },
  vec4u_ptr_composite_swizzle_assign: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  bufferView<vec4u>(&v, 0).zxy = vec3u(42);
}`,
    requires: ['pointer_composite_access', 'swizzle_assignment']
  },
  compound_assign: {
    code: `
var<workgroup> v : buffer<256>;
@compute @workgroup_size(1)
fn main() {
  *bufferView<u32>(&v, 0) += 123u;
}`
  }
};

g.test('lhs_call').
desc('Validate that bufferView can be on the LHS of an assignment').
params((u) => u.combine('case', keysOf(kLHSCallCases))).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');

  const testcase = kLHSCallCases[t.params.case];
  let valid = true;
  const features = testcase.requires ?? [];
  features.forEach((f) => {
    valid &&= t.hasLanguageFeature(f);
  });
  t.expectCompileResult(valid, testcase.code);
});