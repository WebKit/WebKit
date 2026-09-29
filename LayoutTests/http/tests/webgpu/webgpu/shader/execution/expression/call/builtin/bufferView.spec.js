/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Execution tests for bufferView
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf } from '../../../../../../common/util/data_tables.js';
import { assert } from '../../../../../../common/util/util.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../../../gpu_test.js';
import { Type } from '../../../../../util/conversion.js';

import {
  kBufferSizes,
  kArrayLengthTypes,
  kOffsets,
  kCalls,
  kStructDecls,
  isValidArrayLengthCase,
  calculateArrayLength,
  runLengthTest,
  kLayoutCases,
  runReadLayoutTest,
  runWriteLayoutTest,
  runReadWriteTest,
  kMixedTypeOps,
  kMixedTypeIdx,
  kMixedTypePairs,
  kMixedTypeOverlaps,
  kMixedTypeBuffers,

  mixedTypeName,
  mixedTypeWords,
  runMixedTypeAliasingTest } from
'./buffer_view_utils.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);

g.test('array_length').
desc('Tests arrayLength from a bufferView').
params((u) =>
u.
combine('type', keysOf(kArrayLengthTypes)).
combine('aspace', ['workgroup', 'storage', 'uniform']).
beginSubcases().
combine('sized', [false, true]).
combine('override', [false, true]).
combine('bufferSize', kBufferSizes).
combine('dynamic_offset', [0, 256]).
filter((t) => {
  if (t.aspace === 'workgroup' && t.dynamic_offset !== 0) {
    return false;
  }
  if (t.aspace !== 'workgroup' && t.override === true) {
    return false;
  }
  return t.sized || t.aspace === 'storage';
})
).
fn((t) => {
  const testcase = kArrayLengthTypes[t.params.type];
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  if (t.params.aspace === 'uniform' && testcase.uniformStdLayout === true) {
    t.skipIfLanguageFeatureNotSupported('uniform_buffer_standard_layout');
  }
  if (testcase.f16) {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }

  const bSize = t.params.override ? 'bufferSize' : `${t.params.bufferSize}`;
  const buffer_ty = `buffer${t.params.sized ? `<${bSize}>` : ``}`;
  let decl = '';
  switch (t.params.aspace) {
    case 'workgroup':
      decl = `var<workgroup> v : ${buffer_ty};\n@group(0) @binding(0) var<storage> dummy : array<u32>;`;
      break;
    case 'uniform':
      decl = `@group(0) @binding(0) var<uniform> v : ${buffer_ty};`;
      break;
    case 'storage':
      decl = `@group(0) @binding(0) var<storage> v : ${buffer_ty};`;
      break;
  }

  const values = [];
  const offsets = [];
  const sizes = [0];
  for (const offset of kOffsets) {
    // Skip any case that results in an invalid memory reference.
    // Split the writes up one per thread.
    if (isValidArrayLengthCase(testcase, offset, 0, t.params.bufferSize)) {
      values.push(calculateArrayLength(testcase, offset, 0, t.params.bufferSize));
      // Setup the offset and size buffer values so they can be indexed by invocation id.
      offsets.push(offset);
    }
  }
  assert(values.length > 0, 'no tests run');

  const access = testcase.access ?? '';
  const addrOf = access === '' ? '' : '&';

  const enables = testcase.f16 === true ? 'enable f16;' : '';

  const wgsl = `
${enables}

${kStructDecls}

override bufferSize : u32 = 0u;
${decl}
@group(0) @binding(1) var<storage> offsets : array<u32>;
@group(0) @binding(2) var<storage> sizes : array<u32>; // unused
@group(0) @binding(3) var<storage, read_write> out : array<u32>;

override wgx : u32;
@compute @workgroup_size(wgx)
fn main(@builtin(global_invocation_id) gid : vec3u) {
  ${t.params.aspace === 'workgroup' ? '_ = dummy[0];' : ''}

  if gid.x >= ${values.length} {
    return;
  }

  out[gid.x] = arrayLength(${addrOf}bufferView<${testcase.type}>(&v, offsets[gid.x])${access});
}`;

  runLengthTest(
    t,
    wgsl,
    t.params.aspace === 'uniform' ? GPUBufferUsage.UNIFORM : GPUBufferUsage.STORAGE,
    t.params.bufferSize,
    t.params.dynamic_offset,
    offsets,
    sizes,
    values
  );
});

g.test('array_length,functions').
desc('Tests arrayLength from a bufferView through various function calls').
params((u) =>
u.
combine('type', keysOf(kArrayLengthTypes)).
combine('aspace', ['workgroup', 'storage', 'uniform']).
beginSubcases().
combine('call', kCalls).
combine('bufferSize', kBufferSizes).
combine('dynamic_offset', [0, 256]).
filter((t) => {
  if (t.type === 'array_S_2' || t.type === 'mat4x4f' || t.type === 'mat4x3f') {
    return t.bufferSize > 128;
  }
  return t.aspace !== 'workgroup' || t.dynamic_offset === 0;
})
).
fn((t) => {
  const testcase = kArrayLengthTypes[t.params.type];
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  t.skipIfLanguageFeatureNotSupported('unrestricted_pointer_parameters');
  if (t.params.aspace === 'uniform' && testcase.uniformStdLayout === true) {
    t.skipIfLanguageFeatureNotSupported('uniform_buffer_standard_layout');
  }
  if (testcase.f16) {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }

  const buffer_ty = `buffer<${t.params.bufferSize}>`;
  let decl = '';
  switch (t.params.aspace) {
    case 'workgroup':
      decl = `var<workgroup> v : ${buffer_ty};\n@group(0) @binding(0) var<storage> dummy : array<u32>;`;
      break;
    case 'uniform':
      decl = `@group(0) @binding(0) var<uniform> v : ${buffer_ty};`;
      break;
    case 'storage':
      decl = `@group(0) @binding(0) var<storage> v : ${buffer_ty};`;
      break;
  }

  const values = [];
  const offsets = [];
  const sizes = [0];
  for (const offset of kOffsets) {
    const usedSize = t.params.call === 'unsized' ? t.params.bufferSize : t.params.bufferSize / 2;
    // Skip any case that results in an invalid memory reference.
    // Split the writes up one per thread.
    if (isValidArrayLengthCase(testcase, offset, 0, usedSize)) {
      values.push(calculateArrayLength(testcase, offset, 0, usedSize));
      // Setup the offset and size buffer values so they can be indexed by invocation id.
      offsets.push(offset);
    }
  }

  assert(values.length > 0, 'no tests run');

  const access = testcase.access ?? '';
  const addrOf = access === '' ? '' : '&';

  const enables = testcase.f16 === true ? 'enable f16;' : '';

  const wgsl = `
${enables}

${kStructDecls}

fn unsizedLength(p : ptr<${t.params.aspace}, buffer>, gidx : u32) -> u32 {
  return arrayLength(${addrOf}bufferView<${testcase.type}>(p, offsets[gidx])${access});
}

fn sizedLength(p : ptr<${t.params.aspace}, buffer<${t.params.bufferSize / 2}>>, gidx : u32) -> u32 {
  return arrayLength(${addrOf}bufferView<${testcase.type}>(p, offsets[gidx])${access});
}

fn sized_indirectLength(p : ptr<${t.params.aspace}, buffer<${
  t.params.bufferSize / 2
  }>>, gidx : u32) -> u32 {
  return unsizedLength(p, gidx);
}

fn sized_sized_indirectLength(p : ptr<${t.params.aspace}, buffer<${
  t.params.bufferSize
  }>>, gidx : u32) -> u32 {
  return sizedLength(p, gidx);
}

override bufferSize : u32 = 0u;
${decl}
@group(0) @binding(1) var<storage> offsets : array<u32>;
@group(0) @binding(2) var<storage> sizes : array<u32>; // unused
@group(0) @binding(3) var<storage, read_write> out : array<u32>;

override wgx : u32;
@compute @workgroup_size(wgx)
fn main(@builtin(global_invocation_id) gid : vec3u) {
  ${t.params.aspace === 'workgroup' ? '_ = dummy[0];' : ''}

  if gid.x >= ${values.length} {
    return;
  }

  out[gid.x] = ${t.params.call}Length(&v, gid.x);
}`;

  runLengthTest(
    t,
    wgsl,
    t.params.aspace === 'uniform' ? GPUBufferUsage.UNIFORM : GPUBufferUsage.STORAGE,
    t.params.bufferSize,
    t.params.dynamic_offset,
    offsets,
    sizes,
    values
  );
});

g.test('read_layout').
desc('Test reading memory layout from a bufferView').
params((u) =>
u.
combine('case', keysOf(kLayoutCases)).
beginSubcases().
combine('aspace', ['storage', 'ro_storage', 'uniform', 'workgroup']).
combine('offset', kOffsets).
filter((t) => {
  const testcase = kLayoutCases[t.case];
  if ((t.offset & testcase.align - 1) !== 0) {
    return false;
  }
  return testcase.offset + t.offset < 252;
})
).
fn((t) => {
  const testcase = kLayoutCases[t.params.case];
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  if (t.params.aspace === 'uniform' && testcase.uniformStdLayoutView === true) {
    t.skipIfLanguageFeatureNotSupported('uniform_buffer_standard_layout');
  }
  if (testcase.f16) {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }

  const enables = testcase.f16 === true ? 'enable f16;' : '';
  const v = t.params.aspace === 'workgroup' ? 'wg_var' : 'in';

  const wgsl = `
${enables}

${testcase.decl ?? ''}

@group(0) @binding(0)
var<${t.params.aspace === 'uniform' ? 'uniform' : 'storage'}${
  t.params.aspace === 'storage' ? ', read_write' : ''
  }> in : buffer<256 * 4>;

@group(0) @binding(1) var<storage, read_write> out : u32;

var<workgroup> wg_var : buffer<256 * 4>;

@compute @workgroup_size(1)
fn main() {
  let in_ptr = bufferView<array<vec4u, 256 / 4>>(&in, 0);
  let wg_ptr = bufferView<array<vec4u, 256 / 4>>(&wg_var, 0);
  for (var i = 0; i < 256 / 4; i++) {
    (*wg_ptr)[i] = (*in_ptr)[i];
  }

  workgroupBarrier();

  let p = bufferView<${testcase.type}>(&${v}, ${t.params.offset});
  out = u32((*p)${testcase.access});
}
`;

  runReadLayoutTest(t, testcase, wgsl, t.params.aspace, t.params.offset);
});

g.test('write_layout').
desc('Test writing memory layout via a bufferView').
params((u) =>
u.
combine('case', keysOf(kLayoutCases)).
beginSubcases().
combine('assign', ['let', 'call']).
combine('aspace', ['storage', 'workgroup']).
combine('offset', kOffsets).
filter((t) => {
  const testcase = kLayoutCases[t.case];
  if ((t.offset & testcase.align - 1) !== 0) {
    return false;
  }
  return testcase.offset + t.offset < 252;
})
).
fn((t) => {
  const testcase = kLayoutCases[t.params.case];
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  if (testcase.f16) {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }

  const enables = testcase.f16 === true ? 'enable f16;' : '';
  const v = t.params.aspace === 'workgroup' ? 'wg_var' : 'out';
  const call_expr = `bufferView<${testcase.type}>(&${v}, ${t.params.offset})`;

  let post_assign = '';
  if (t.params.aspace === 'workgroup') {
    post_assign = `
  workgroupBarrier();

  let out_ptr = bufferView<array<u32>>(&out, 0);
  let wg_ptr = bufferView<array<u32>>(&wg_var, 0);
  for (var i = 0; i < 256; i++) {
    (*out_ptr)[i] = (*wg_ptr)[i];
  }
`;
  }

  const wgsl = `
${enables}

${testcase.decl ?? ''}

@group(0) @binding(0) var<storage> in : u32;
@group(0) @binding(1) var<storage, read_write> out : buffer<256 * 4>;


var<workgroup> wg_var : buffer<256 * 4>;

@compute @workgroup_size(1)
fn main() {
  let val = ${testcase.f16 ? 'f16' : testcase.f32 ? 'f32' : 'u32'}(in);
  let ptr = ${call_expr};
  (*${t.params.assign === 'call' ? call_expr : 'ptr'})${testcase.access} = val;

  ${post_assign}
}
`;

  runWriteLayoutTest(t, testcase, wgsl, t.params.offset);
});

g.test('read').
desc('Test reading various types from bufferView').
params((u) =>
u.
combine('base_type', ['u32', 'i32', 'f32', 'f16']).
beginSubcases().
combine('wrap', ['none', 'vector', 'array', 'matrix']).
combine('width', [1, 2, 3, 4]).
combine('aspace', ['workgroup', 'storage', 'ro_storage', 'uniform']).
combine('offset', [0, 4, 8, 12, 16, 32, 48, 64]).
filter((t) => {
  if (t.wrap !== 'none' && t.width === 1) {
    return false;
  }
  if (t.wrap === 'none' && t.width !== 1) {
    return false;
  }
  if (t.wrap === 'matrix' && t.base_type !== 'f32' && t.base_type !== 'f16') {
    return false;
  }
  if (t.aspace === 'uniform' && t.wrap === 'array') {
    return false;
  }
  const ty = Type[t.base_type];
  let align = ty.alignment;
  switch (t.wrap) {
    case 'vector':
      align = Type.vec(t.width, ty).alignment;
      break;
    case 'array':
      align = Type.array(2, ty).alignment;
      break;
    case 'matrix':
      align = Type.mat(t.width, 2, ty).alignment;
      break;
    case 'none':
      break;
  }
  if (t.offset % align !== 0) {
    return false;
  }
  return true;
})
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  if (t.params.base_type === 'f16') {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }

  const ele_ty = Type[t.params.base_type];
  let ty = ele_ty;
  switch (t.params.wrap) {
    case 'vector':
      ty = Type.vec(t.params.width, ele_ty);
      break;
    case 'array':
      ty = Type.array(t.params.width, ele_ty);
      break;
    case 'matrix':
      ty = Type.mat(t.params.width, 2, ele_ty);
      break;
    case 'none':
      break;
  }

  const bufferSize = 128;
  const enables = t.params.base_type === 'f16' ? 'enable f16;' : '';
  const v = t.params.aspace === 'workgroup' ? 'wg_var' : 'in';
  const wgsl = `
${enables}


@group(0) @binding(0)
var<${t.params.aspace === 'uniform' ? 'uniform' : 'storage'}${
  t.params.aspace === 'storage' ? ', read_write' : ''
  }> in : buffer<${bufferSize}>;

@group(0) @binding(1) var<storage, read_write> out : ${ty.toString()};

var<workgroup> wg_var : buffer<${bufferSize}>;

@compute @workgroup_size(1)
fn main() {
  let in_ptr = bufferView<array<vec4u, ${bufferSize} / 16>>(&in, 0);
  let wg_ptr = bufferView<array<vec4u, ${bufferSize} / 16>>(&wg_var, 0);
  for (var i = 0; i < 256 / 4; i++) {
    (*wg_ptr)[i] = (*in_ptr)[i];
  }

  workgroupBarrier();

  let p = bufferView<${ty.toString()}>(&${v}, ${t.params.offset});
  out = (*p);
}
`;

  runReadWriteTest(true, t, wgsl, ele_ty, ty, t.params.aspace, t.params.offset, bufferSize);
});

g.test('write').
desc('Test writing various types via bufferView').
params((u) =>
u.
combine('base_type', ['u32', 'i32', 'f32', 'f16']).
beginSubcases().
combine('wrap', ['none', 'vector', 'array', 'matrix']).
combine('width', [1, 2, 3, 4]).
combine('aspace', ['workgroup', 'storage']).
combine('offset', [0, 4, 8, 12, 16, 32, 48, 64]).
combine('assign', ['let', 'call']).
combine('swizzle', [false, true]).
filter((t) => {
  if (t.wrap !== 'none' && t.width === 1) {
    return false;
  }
  if (t.wrap === 'none' && t.width !== 1) {
    return false;
  }
  if (t.wrap === 'matrix' && t.base_type !== 'f32' && t.base_type !== 'f16') {
    return false;
  }
  if (t.wrap !== 'vector' && t.swizzle) {
    return false;
  }
  const ty = Type[t.base_type];
  let align = ty.alignment;
  switch (t.wrap) {
    case 'vector':
      align = Type.vec(t.width, ty).alignment;
      break;
    case 'array':
      align = Type.array(2, ty).alignment;
      break;
    case 'matrix':
      align = Type.mat(t.width, 2, ty).alignment;
      break;
    case 'none':
      break;
  }
  if (t.offset % align !== 0) {
    return false;
  }
  return true;
})
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  if (t.params.base_type === 'f16') {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }
  if (t.params.swizzle) {
    t.skipIfLanguageFeatureNotSupported('swizzle_assignment');
  }

  let swizzle = '';
  const ele_ty = Type[t.params.base_type];
  let ty = ele_ty;
  switch (t.params.wrap) {
    case 'vector':
      ty = Type.vec(t.params.width, ele_ty);
      if (t.params.swizzle) {
        swizzle = '.xyzw'.substring(0, 1 + t.params.width);
      }
      break;
    case 'array':
      ty = Type.array(t.params.width, ele_ty);
      break;
    case 'matrix':
      ty = Type.mat(t.params.width, 2, ele_ty);
      break;
    case 'none':
      break;
  }

  const bufferSize = 128;
  const enables = t.params.base_type === 'f16' ? 'enable f16;' : '';
  const v = t.params.aspace === 'workgroup' ? 'wg_var' : 'out';

  let post_assign = '';
  if (t.params.aspace === 'workgroup') {
    post_assign = `
  workgroupBarrier();

  let out_ptr = bufferView<array<u32>>(&out, 0);
  let wg_ptr = bufferView<array<u32>>(&wg_var, 0);
  for (var i = 0; i < 256; i++) {
    (*out_ptr)[i] = (*wg_ptr)[i];
  }
`;
  }

  const call_expr = `bufferView<${ty.toString()}>(&${v}, ${t.params.offset})`;
  const wgsl = `
${enables}

@group(0) @binding(0) var<storage> in : ${ty.toString()};
@group(0) @binding(1) var<storage, read_write> out : buffer<${bufferSize}>;


var<workgroup> wg_var : buffer<${bufferSize}>;

@compute @workgroup_size(1)
fn main() {
  let ptr = ${call_expr};
  (*${t.params.assign === 'call' ? call_expr : 'ptr'})${swizzle} = in;

  ${post_assign}
}
`;

  runReadWriteTest(false, t, wgsl, ele_ty, ty, t.params.aspace, t.params.offset, bufferSize);
});

g.test('mixed_types_aliasing').
desc(
  `Test that differently typed pointers formed with bufferView behave correctly when they are views
of overlapping bytes of a buffer, and both are formed and used within a single function.

Aliasing within a function is valid WGSL, so an implementation must not assume that differently
typed views do not alias (e.g. by applying C++ style type-based alias analysis).

 * 'pair' selects the types of the two views, covering combinations of scalars, vectors, and
   structures (with and without padding), including f16 types.
 * 'overlap' selects whether the views start at the same byte, partially overlap, or only overlap
   padding.
 * 'offset' selects whether the byte offset passed to bufferView is a constant or a runtime value.
 * If 'aliased' is false, the second view is formed on a different buffer.`
).
params((u) =>
u.
combine('buffer', kMixedTypeBuffers).
combine('pair', keysOf(kMixedTypePairs)).
combine('overlap', kMixedTypeOverlaps).
filter((t) => {
  const pair = kMixedTypePairs[t.pair];
  return mixedTypeWords(pair.int, pair.other, t.overlap) !== undefined;
}).
combine('offset', ['constant', 'dynamic']).
combine('aliased', [true, false]).
beginSubcases().
combine('op', keysOf(kMixedTypeOps))
).
fn((t) => {
  const pair = kMixedTypePairs[t.params.pair];
  const words = mixedTypeWords(pair.int, pair.other, t.params.overlap);
  // The dynamic offset adds 'input.p[3] * 16' bytes, which preserves the alignment of all types.
  const dynamicWords = 4 * kMixedTypeIdx;
  const view = (type, buffer, word) => {
    const ty = mixedTypeName(type);
    return t.params.offset === 'constant' ?
    `bufferView<${ty}>(&${buffer}, ${word * 4}u)` :
    `bufferView<${ty}>(&${buffer}, ${(word - dynamicWords) * 4}u + u32(input.p[3]) * 16u)`;
  };
  runMixedTypeAliasingTest(t, {
    buffer: t.params.buffer,
    int: pair.int,
    other: pair.other,
    intWord: words.intWord,
    otherWord: words.otherWord,
    view,
    aliased: t.params.aliased,
    op: kMixedTypeOps[t.params.op]
  });
});