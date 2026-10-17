/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for bufferArrayView
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
  ${t.params.must_use ? 'let p = ' : ''}bufferArrayView<array<u32>>(&v, 0, 64);
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
combine('ptr', [false, true]).
combine('array', [false, true])
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const template_type = kTypes[t.params.template_type];
  const return_type = kTypes[t.params.return_type];
  let ret_type = return_type.toString();
  let temp_type = template_type.toString();
  if (t.params.array) {
    ret_type = `array<${return_type.toString()}>`;
    temp_type = `array<${temp_type}>`;
  }
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
  let res : ${ret_type} = bufferArrayView<${temp_type}>(&v, 0, 128);
}`;

  t.expectCompileResult(t.params.ptr && t.params.array && template_type === return_type, wgsl);
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
  let p : ptr<${aspace}, array<u32>${access}> = bufferArrayView<array<u32>>(${
  t.params.ptr ? '&' : ''
  }${t.params.type}, 0, 128);
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
  let p = bufferArrayView<array<u32>>(&v, ${type.create(0).wgsl()}, 128);
}`;

  t.expectCompileResult(
    type === Type.abstractInt || type === Type.u32 || type === Type.i32,
    wgsl
  );
});

g.test('size_type').
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
  let p = bufferArrayView<array<u32>>(&v, 0, ${type.create(128).wgsl()});
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
  let p = bufferArrayView<array<vec4u>>(&v, 0, 16);
}`,
    valid: true
  },
  storage_buffer: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer<16>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 0, 16);
}`,
    valid: true
  },
  uniform_buffer: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<16>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 0, 16);
}`,
    valid: true
  },
  workgroup_buffer: {
    code: `
var<workgroup> v : buffer<16>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 0, 16);
}`,
    valid: true
  },
  const_size_too_small_for_type: {
    code: `
@group(0) @binding(0) var<storage> v : buffer;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 0, 12);
}`,
    valid: false
  },
  buffer_too_small_for_const_size: {
    code: `
@group(0) @binding(0) var<storage> v : buffer<12>;
fn foo() {
  let p = bufferArrayView<array<u32>>(&v, 0, 16);
}`,
    valid: false
  },
  buffer_too_small_for_type: {
    code: `
@group(0) @binding(0) var<storage> v : buffer<12>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 0, 16);
}`,
    valid: false
  },
  const_size_too_small_for_const_offset_and_type: {
    code: `
var<workgroup> v : buffer<128>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 16, 30);
}`,
    valid: false
  },
  buffer_size_too_small_for_const_offset_and_type: {
    code: `
var<workgroup> v : buffer<30>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 16, 128);
}`,
    valid: false
  },
  override_size_too_small_for_type: {
    code: `
override size : u32;
@group(0) @binding(0) var<storage> v : buffer;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 0, size);
}`,
    valid: 'pipeline',
    constants: { size: 12 }
  },
  buffer_too_small_for_override_size: {
    code: `
override buffer_size : u32;
override size : u32;
var<workgroup> v : buffer<buffer_size>;
fn foo() {
  let p = bufferArrayView<array<u32>>(&v, 0, size);
}`,
    valid: 'pipeline',
    constants: { buffer_size: 12, size: 16 }
  },
  override_buffer_too_small_for_type: {
    code: `
override buffer_size : u32;
override size : u32;
var<workgroup> v : buffer<buffer_size>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, 0, size);
}`,
    valid: 'pipeline',
    constants: { buffer_size: 12, size: 16 }
  },
  override_size_too_small_for_override_offset_and_type: {
    code: `
override offset : u32;
override size : u32;
@group(0) @binding(0) var<uniform> v : buffer<128>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, offset, size);
}`,
    valid: 'pipeline',
    constants: { offset: 16, size: 30 }
  },
  buffer_size_too_small_for_override_offset_and_type: {
    code: `
override buffer_size : u32;
override offset : u32;
var<workgroup> v : buffer<buffer_size>;
fn foo() {
  let p = bufferArrayView<array<vec4u>>(&v, offset, 128);
}`,
    valid: 'pipeline',
    constants: { buffer_size: 30, offset: 16 }
  },
  const_offset_not_aligned: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
fn foo() {
  let p = bufferarrayview<array<vec4u>>(&v, 12, 128);
}`,
    valid: false
  },
  override_offset_not_aligned: {
    code: `
override offset : u32;
@group(0) @binding(0) var<storage, read_write> v : buffer;
fn foo() {
  let p = bufferarrayview<array<vec4u>>(&v, offset, 128);
}`,
    valid: false,
    constants: { offset: 12 }
  },
  const_offset_plus_type_size_out_of_range: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
struct S {
  a: vec4u,
  b: vec4f,
}
fn foo() {
  let p = buffer<array<S>>(&v, 4294967279, 128);
}`,
    valid: false
  },
  override_offset_plus_type_size_out_of_range: {
    code: `
override offset : u32;
@group(0) @binding(0) var<storage, read_write> v : buffer;
struct S {
  a: vec4u,
  b: vec4f,
}
fn foo() {
  let p = buffer<array<S>>(&v, offset, 128);
}`,
    valid: false,
    constants: { offset: 4294967279 }
  },
  const_multiple_of_stride: {
    code: `
struct S {
  a : vec4u,
  b : array<vec2u>,
}
var<workgroup> v : buffer<256>;
fn foo() {
  let p = bufferArrayView<S>(&v, 0, 24);
}`,
    valid: true
  },
  const_not_multiple_of_stride: {
    code: `
struct S {
  a : vec4u,
  b : array<vec2u>,
}
var<workgroup> v : buffer<256>;
fn foo() {
  let p = bufferArrayView<S>(&v, 0, 26);
}`,
    valid: false
  },
  override_not_multiple_of_stride: {
    code: `
override size : u32;
struct S {
  a : vec4u,
  b : array<vec2u>,
}
var<workgroup> v : buffer<256>;
fn foo() {
  let p = bufferArrayView<S>(&v, 0, size);
}`,
    valid: 'pipeline',
    constants: { size: 26 }
  },
  const_buffer_too_small_through_unsized_function: {
    code: `
@group(0) @binding(0) var<storage> buffer<16>;
fn bar(p : ptr<storage, buffer>) {
  let q = bufferArrayView<array<u32>>(p, 0, 32);
}
fn foo() {
  bar(&v);
}`,
    valid: false,
    ptr_param: true
  },
  override_buffer_too_small_through_unsized_function: {
    code: `
override size : u32;
@group(0) @binding(0) var<storage> v : buffer<16>;
fn bar(p : ptr<storage, buffer>) {
  let q = bufferArrayView<array<u32>>(p, 0, size);
}
fn foo() {
  bar(&v);
}`,
    valid: 'pipeline',
    constants: { size: 32 },
    ptr_param: true
  },
  sized_buffer_to_smaller_buffer_too_small: {
    code: `
var<workgroup> v : buffer<256>;
fn bar(p : ptr<workgroup, buffer<16>>) {
  let q = bufferArrayView<array<u32>>(q, 16, 4);
}
fn foo() {
  bar(&v);
}`,
    valid: false,
    ptr_param: true
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
  u32_array: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  (*bufferArrayView<array<u32>>(&v, 0, 128))[0] = 123u;
}`
  },
  f32_vector_letter: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  (*bufferArrayView<array<vec4f>>(&v, 0, 128))[0].x = 13.3f;
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
  (*bufferArrayView<array<S>>(&v, 0, 128))[0].member_a = 13.3f;
}`
  },
  u32_array_ptr_composite: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  bufferArrayView<array<u32>>(&v, 0, 128)[0] = 123u;
}`,
    requires: ['pointer_composite_access']
  },
  vec4u_swizzle_assign: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  (*bufferArrayView<array<vec4u>>(&v, 0, 128))[0].zxy = vec3u(42);
}`,
    requires: ['swizzle_assignment']
  },
  vec4u_ptr_composite_swizzle_assign: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  bufferArrayView<array<vec4u>>(&v, 0, 128)[0].zxy = vec3u(42);
}`,
    requires: ['pointer_composite_access', 'swizzle_assignment']
  },
  compound_assign: {
    code: `
var<workgroup> v : buffer<256>;
@compute @workgroup_size(1)
fn main() {
  (*bufferArrayView<array<u32>>(&v, 0, 128))[0] += 123u;
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