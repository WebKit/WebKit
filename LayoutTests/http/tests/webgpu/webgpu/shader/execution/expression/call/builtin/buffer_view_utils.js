/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import { assert, iterRange,
  typedArrayParam,
  typedArrayFromParam } from

'../../../../../../common/util/util.js';

import {
  Type,

  ArrayType,
  MatrixType,
  VectorType,
  float32ToFloat16Bits } from
'../../../../../util/conversion.js';

export const kBufferSizes = [128, 256, 512, 1024];
export const kOffsets = [0, 1, 2, 3, 4, 8, 12, 16, 32, 100, 156, 480, 768];
export const kSizes = [...kBufferSizes, 32, 48, 64];
export const kCalls = ['unsized', 'sized', 'sized_indirect', 'sized_sized_indirect'];











/**
 * Return true if the case is valid
 *
 * @param type The testcase
 * @param offset The offset to the view call
 * @param size The size to the view call
 * @param bufferSize The size of the buffer
 * @returns true if case is valid.
 */
export function isValidArrayLengthCase(
type,
offset,
size,
bufferSize)
{
  if (size === 0) {
    // bufferView case
    return offset + (type.arrayOffset ?? 0) + type.stride <= bufferSize;
  } else {
    // bufferArrayView case
    if ((type.arrayOffset ?? 0) + type.stride > size) {
      return false;
    }
    return offset + size <= bufferSize;
  }
}

/**
 * Calculates the return length in elements
 *
 * @param type The testcase
 * @param offset The offset to the view call
 * @param size The size to the view call
 * @param bufferSize The buffer size
 * @returns The number of elements in the arrayLength call
 */
export function calculateArrayLength(
type,
offset,
size,
bufferSize)
{
  const alignOffset = offset & ~(type.align - 1);
  if (size === 0) {
    // bufferView case
    return Math.floor((bufferSize - alignOffset - (type.arrayOffset ?? 0)) / type.stride);
  } else {
    // bufferArrayView case
    return Math.floor((size - (type.arrayOffset ?? 0)) / type.stride);
  }
}

export const kStructDecls = `
struct S {
  a: vec4u,
  b: u32,
}
struct T {
  a : u32,
  b : array<u32, 5>
}
struct S_arr {
  a : vec4u,
  b : array<u32>,
}
struct T_arr {
  a : S,
  b : array<T>,
}
`;

export const kArrayLengthTypes = {
  // Scalars and vectors
  u32: {
    type: 'array<u32>',
    align: 4,
    stride: 4,
    uniformStdLayout: true
  },
  vec2f: {
    type: 'array<vec2f>',
    align: 8,
    stride: 8,
    uniformStdLayout: true
  },
  vec3i: {
    type: 'array<vec3i>',
    align: 16,
    stride: 16
  },
  vec4u: {
    type: 'array<vec4u>',
    align: 16,
    stride: 16
  },
  f16: {
    type: 'array<f16>',
    align: 2,
    stride: 2,
    f16: true,
    uniformStdLayout: true
  },
  vec2h: {
    type: 'array<vec2h>',
    align: 4,
    stride: 4,
    f16: true,
    uniformStdLayout: true
  },
  vec3h: {
    type: 'array<vec3h>',
    align: 8,
    stride: 8,
    f16: true,
    uniformStdLayout: true
  },
  vec4h: {
    type: 'array<vec4h>',
    align: 8,
    stride: 8,
    f16: true,
    uniformStdLayout: true
  },
  // Matrices
  mat2x2f: {
    type: 'array<mat2x2f>',
    align: 8,
    stride: 16
  },
  mat2x3f: {
    type: 'array<mat2x3f>',
    align: 16,
    stride: 32
  },
  mat2x4f: {
    type: 'array<mat2x4f>',
    align: 16,
    stride: 32
  },
  mat4x2f: {
    type: 'array<mat4x2f>',
    align: 8,
    stride: 32
  },
  mat4x3f: {
    type: 'array<mat4x3f>',
    align: 16,
    stride: 64
  },
  mat4x4f: {
    type: 'array<mat4x4f>',
    align: 16,
    stride: 64
  },
  mat3x2h: {
    type: 'array<mat3x2h>',
    align: 4,
    stride: 12,
    f16: true
  },
  mat3x3h: {
    type: 'array<mat3x3h>',
    align: 8,
    stride: 24,
    f16: true,
    uniformStdLayout: true
  },
  mat3x4h: {
    type: 'array<mat3x4h>',
    align: 8,
    stride: 24,
    f16: true,
    uniformStdLayout: true
  },
  // Structs without arrays
  S: {
    type: 'array<S>',
    align: 16,
    stride: 32
  },
  T: {
    type: 'array<T>',
    align: 4,
    stride: 24,
    uniformStdLayout: true
  },
  // Sized arrays
  array_u32_4: {
    type: 'array<array<u32, 4>>',
    align: 4,
    stride: 16,
    uniformStdLayout: true
  },
  array_f16_6: {
    type: 'array<array<f16, 6>>',
    align: 2,
    stride: 12,
    f16: true,
    uniformStdLayout: true
  },
  array_vec2f_2: {
    type: 'array<array<vec2f, 2>>',
    align: 8,
    stride: 16,
    uniformStdLayout: true
  },
  array_S_2: {
    type: 'array<array<S, 2>>',
    align: 16,
    stride: 64
  },
  array_T_2: {
    type: 'array<array<T, 2>>',
    align: 4,
    stride: 48
  },
  // Structs with runtime arrays
  S_arr: {
    type: 'S_arr',
    access: '.b',
    align: 16,
    stride: 4,
    arrayOffset: 16
  },
  T_arr: {
    type: 'T_arr',
    access: '.b',
    align: 16,
    stride: 24,
    arrayOffset: 32
  }
};

/**
 * Run arrayLength tests for bufferView and bufferArrayView
 *
 * @param t The test
 * @param wgsl The shader code. Interface requirements:
 *             * Overrides:
 *               * wgx: workgroup size x
 *               * bufferSize: size of the buffer
 *             * (0, 0): input buffer with 'usage' usage
 *             * (0, 1): read-only-storage-buffer for offsets
 *             * (0, 2): read-only-storage-buffer for sizes
 *             * (0, 3): storage-buffer for output
 * @param usage The test buffer usage
 * @param bufferSize The size of the test buffer
 * @param dynOffset The size of the dynamic offset for the test buffer
 * @param offsets The values for the offset buffer
 * @param sizes The values for the size buffer
 * @param values The expected results
 */
export function runLengthTest(
t,
wgsl,
usage,
bufferSize,
dynOffset,
offsets,
sizes,
values)
{
  const fullBufferSize = bufferSize + dynOffset;
  const inputBuffer = t.createBufferTracked({
    size: fullBufferSize,
    usage
  });
  const offsetBuffer = t.makeBufferWithContents(
    new Uint32Array(offsets),
    GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  );
  const sizeBuffer = t.makeBufferWithContents(
    new Uint32Array(sizes),
    GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  );
  const outputBuffer = t.createBufferTracked({
    size: values.length * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  const bgLayout = t.device.createBindGroupLayout({
    entries: [
    {
      binding: 0,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: usage === GPUBufferUsage.UNIFORM ? 'uniform' : 'read-only-storage',
        hasDynamicOffset: dynOffset !== 0
      }
    },
    {
      binding: 1,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'read-only-storage'
      }
    },
    {
      binding: 2,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'read-only-storage'
      }
    },
    {
      binding: 3,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'storage'
      }
    }]

  });

  // Limit the number of invocations per workgroup to 128 to fit the default compat limits. Launch extra workgroups to fit all the required invocations.
  const wgx = Math.min(values.length, 128);
  const num_wgs = Math.ceil(values.length / wgx);

  const pipelineLayout = t.device.createPipelineLayout({
    bindGroupLayouts: [bgLayout]
  });
  const pipeline = t.device.createComputePipeline({
    layout: pipelineLayout,
    compute: {
      module: t.device.createShaderModule({ code: wgsl }),
      constants: { wgx, bufferSize }
    }
  });

  const bg = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
    {
      binding: 0,
      resource: {
        buffer: inputBuffer,
        size: bufferSize
      }
    },
    {
      binding: 1,
      resource: {
        buffer: offsetBuffer
      }
    },
    {
      binding: 2,
      resource: {
        buffer: sizeBuffer
      }
    },
    {
      binding: 3,
      resource: {
        buffer: outputBuffer
      }
    }]

  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bg, dynOffset === 0 ? [] : [dynOffset]);
  pass.dispatchWorkgroups(num_wgs, 1, 1);
  pass.end();
  t.queue.submit([encoder.finish()]);

  t.expectGPUBufferValuesEqual(outputBuffer, new Uint32Array(values));
}













export const kLayoutCases = {
  vec2u_align8: {
    type: 'S_vec2u_align',
    decl: 'struct S_vec2u_align { x : u32, y : vec2u, }',
    access: '.y[1]',
    align: 8,
    offset: 12,
    uniformStdLayoutArrayView: true
  },
  vec3u_align16: {
    type: 'S_vec3u_align',
    decl: 'struct S_vec3u_align { x : u32, y : vec3u, }',
    access: '.y[2]',
    align: 16,
    offset: 24
  },
  vec4u_align16: {
    type: 'S_vec4u_align',
    decl: 'struct S_vec4u_align { x : u32, y : vec4u, }',
    access: '.y[0]',
    align: 16,
    offset: 16
  },
  struct_align32: {
    type: 'S_align32',
    decl: 'struct S_align32 { x : u32, @align(32) y : u32, }',
    access: '.y',
    align: 32,
    offset: 32
  },
  vec2h_align4: {
    type: 'S_vec2h_align',
    decl: 'struct S_vec2h_align { x : f16, y : vec2h }',
    access: '.y[0]',
    align: 4,
    offset: 4,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  vec3h_align8: {
    type: 'S_vec3h_align',
    decl: 'struct S_vec3h_align { x : f16, y : vec3h }',
    access: '.y.z',
    align: 8,
    offset: 12,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  vec4h_align8: {
    type: 'S_vec4h_align',
    decl: 'struct S_vec4h_align { x : f16, y : vec4h }',
    access: '.y.z',
    align: 8,
    offset: 12,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  vec3i_size12: {
    type: 'S_vec3i_size',
    decl: 'struct S_vec3i_size { x : vec3i, y : u32 }',
    access: '.y',
    align: 16,
    offset: 12
  },
  vec3h_size6: {
    type: 'S_vec3h_size',
    decl: 'struct S_vec3h_size { x : vec3h, y : f16, z : f16 }',
    access: '.z',
    align: 8,
    offset: 8,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  size80: {
    type: 'S_size80',
    decl: 'struct S_size80 { @size(80) x : u32, y : u32 }',
    access: '.y',
    align: 4,
    offset: 80,
    uniformStdLayoutArrayView: true
  },
  mat2x2f_align8: {
    type: 'S_mat2x2f_align',
    decl: 'struct S_mat2x2f_align { x : u32, y : mat2x2f }',
    access: '.y[0][0]',
    align: 8,
    offset: 8,
    f32: true,
    uniformStdLayoutArrayView: true
  },
  mat3x3f_align16: {
    type: 'S_mat3x3f_align',
    decl: 'struct S_mat3x3f_align { x : u32, y : mat3x3f }',
    access: '.y[0][0]',
    align: 16,
    offset: 16,
    f32: true
  },
  mat4x4f_align16: {
    type: 'S_mat4x4f_align',
    decl: 'struct S_mat4x4f_align { x : u32, y : mat4x4f }',
    access: '.y[1][0]',
    align: 16,
    offset: 32,
    f32: true
  },
  mat3x2h_align4: {
    type: 'S_mat3x2h_align',
    decl: 'struct S_mat3x2h_align { x : f16, y : mat3x2h }',
    access: '.y[0][0]',
    align: 4,
    offset: 4,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  mat4x3h_align8: {
    type: 'S_mat4x3h_align',
    decl: 'struct S_mat4x3h_align { x : f16, y : mat4x3h }',
    access: '.y[0][0]',
    align: 8,
    offset: 8,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  mat2x4h_align8: {
    type: 'S_mat2x4h_align',
    decl: 'struct S_mat2x4h_align { x : f16, y : mat2x4h }',
    access: '.y[0][0]',
    align: 8,
    offset: 8,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  mat2x2f_size: {
    type: 'S_mat2x2f_size',
    decl: 'struct S_mat2x2f_size { x : mat2x2f, y : u32 }',
    access: '.y',
    align: 8,
    offset: 16,
    uniformStdLayoutArrayView: true
  },
  mat3x2f_size: {
    type: 'S_mat3x2f_size',
    decl: 'struct S_mat3x2f_size { x : mat3x2f, y : u32 }',
    access: '.y',
    align: 8,
    offset: 24,
    uniformStdLayoutArrayView: true
  },
  mat2x3f_size: {
    type: 'S_mat2x3f_size',
    decl: 'struct S_mat2x3f_size { x : mat2x3f, y : u32 }',
    access: '.y',
    align: 16,
    offset: 32
  },
  mat3x3f_size: {
    type: 'S_mat3x3f_size',
    decl: 'struct S_mat3x3f_size { x : mat3x3f, y : u32 }',
    access: '.y',
    align: 16,
    offset: 48
  },
  mat2x4f_size: {
    type: 'S_mat2x4f_size',
    decl: 'struct S_mat2x4f_size { x : mat2x4f, y : u32 }',
    access: '.y',
    align: 16,
    offset: 32
  },
  mat3x4f_size: {
    type: 'S_mat3x4f_size',
    decl: 'struct S_mat3x4f_size { x : mat3x4f, y : u32 }',
    access: '.y',
    align: 16,
    offset: 48
  },
  mat2x2h_size: {
    type: 'S_mat2x2h_size',
    decl: 'struct S_mat2x2h_size { x : mat2x2h, y : f16 }',
    access: '.y',
    align: 4,
    offset: 8,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  mat4x2h_size: {
    type: 'S_mat4x2h_size',
    decl: 'struct S_mat4x2h_size { x : mat4x2h, y : f16 }',
    access: '.y',
    align: 4,
    offset: 16,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  mat2x3h_size: {
    type: 'S_mat2x3h_size',
    decl: 'struct S_mat2x3h_size { x : mat2x3h, y : f16 }',
    access: '.y',
    align: 8,
    offset: 16,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  mat4x3h_size: {
    type: 'S_mat4x3h_size',
    decl: 'struct S_mat4x3h_size { x : mat4x3h, y : f16 }',
    access: '.y',
    align: 8,
    offset: 32,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  mat2x4h_size: {
    type: 'S_mat2x4h_size',
    decl: 'struct S_mat2x4h_size { x : mat2x4h, y : f16 }',
    access: '.y',
    align: 8,
    offset: 16,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  mat4x4h_size: {
    type: 'S_mat4x4h_size',
    decl: 'struct S_mat4x4h_size { x : mat4x4h, y : f16 }',
    access: '.y',
    align: 8,
    offset: 32,
    f16: true,
    uniformStdLayoutArrayView: true
  },
  struct_size_roundup: {
    type: 'S_struct_size_roundup',
    decl: `struct Inner { x : vec3u }
           struct S_struct_size_roundup { x : Inner, y : u32 }`,
    access: '.y',
    align: 16,
    offset: 16
  },
  struct_inner_size: {
    type: 'S_struct_inner_size',
    decl: `struct Inner { @size(112) x : u32 }
           struct S_struct_inner_size { x : Inner, y : u32 }`,
    access: '.y',
    align: 4,
    offset: 112,
    uniformStdLayoutArrayView: true
  },
  struct_inner_align: {
    type: 'S_struct_inner_align',
    decl: `struct Inner { @align(64) x : u32 }
           struct S_struct_inner_align { x : Inner, y : u32 }`,
    access: '.y',
    align: 64,
    offset: 64
  },
  struct_inner_size_and_align: {
    type: 'S_struct_inner_size_and_align',
    decl: `struct Inner { @align(32) @size(33) x : u32 }
           struct S_struct_inner_size_and_align { x : Inner, y : u32 }`,
    access: '.y',
    align: 32,
    offset: 64
  },
  struct_override_size: {
    type: 'S_struct_override_size',
    decl: `struct Inner { @size(32) x : u32 }
           struct S_struct_override_size { @size(64) x : Inner, y : u32 }`,
    access: '.y',
    align: 4,
    offset: 64,
    uniformStdLayoutArrayView: true
  },
  struct_double_align: {
    type: 'S_struct_double_align',
    decl: `struct Inner { x : u32, @align(32) y : u32 }
           struct S_struct_double_align { x : u32, @align(64) y : Inner }`,
    access: '.y.y',
    align: 64,
    offset: 96
  },
  array_stride_size: {
    type: 'array<S_stride, 4>',
    decl: 'struct S_stride { @size(16) x : u32 }',
    access: '[2].x',
    align: 4,
    offset: 32
  }
};

/**
 * Runs a read test for memory layouts from bufferView and bufferArrayView
 *
 * @param t The GPUTest
 * @param testcase the LayoutCase
 * @param wgsl the shader code
 *             * (0, 0) - uniform or storage buffer
 *             * (0, 1) - storage buffer
 * @param aspace the address space being tested
 * @param offset the offset to the view call
 */
export function runReadLayoutTest(
t,
testcase,
wgsl,
aspace,
offset)
{
  let usage = GPUBufferUsage.COPY_DST;
  if (aspace === 'uniform') {
    usage |= GPUBufferUsage.UNIFORM;
  } else {
    usage |= GPUBufferUsage.STORAGE;
  }

  const kBufferLength = 256;
  // Magic number is 42 in various representations.
  const inMagicNumber = testcase.f16 ? 0x5140 : testcase.f32 ? 0x42280000 : 42;
  const inputBuffer = t.makeBufferWithContents(
    new Uint32Array([
    ...iterRange(kBufferLength, (x) => {
      if (x * 4 === testcase.offset + offset) {
        return inMagicNumber;
      } else {
        return 0;
      }
    })]
    ),
    usage
  );

  const outputBuffer = t.createBufferTracked({
    size: 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  const pipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: {
      module: t.device.createShaderModule({ code: wgsl })
    }
  });

  const bg = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
    {
      binding: 0,
      resource: {
        buffer: inputBuffer
      }
    },
    {
      binding: 1,
      resource: {
        buffer: outputBuffer
      }
    }]

  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bg);
  pass.dispatchWorkgroups(1, 1, 1);
  pass.end();
  t.queue.submit([encoder.finish()]);

  t.expectGPUBufferValuesEqual(outputBuffer, new Uint32Array([42]));
}

/**
 * Runs a write test for memory layout from bufferView and bufferArrayView
 *
 * @param t The GPUTest
 * @param testcase the LayoutCase
 * @param wgsl The shader code
 *             * (0, 0) - a read-only storage buffer
 *             * (0, 1) - a read-write storage buffer
 * @param offset The offset for the view call
 */
export function runWriteLayoutTest(t, testcase, wgsl, offset) {
  const inputBuffer = t.makeBufferWithContents(
    new Uint32Array([42]),
    GPUBufferUsage.COPY_SRC | GPUBufferUsage.STORAGE
  );

  const outputBuffer = t.createBufferTracked({
    size: 256 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  const pipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: {
      module: t.device.createShaderModule({ code: wgsl })
    }
  });

  const bg = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
    {
      binding: 0,
      resource: {
        buffer: inputBuffer
      }
    },
    {
      binding: 1,
      resource: {
        buffer: outputBuffer
      }
    }]

  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bg);
  pass.dispatchWorkgroups(1, 1, 1);
  pass.end();
  t.queue.submit([encoder.finish()]);

  // Magic number is 42 in various representations.
  const outMagicNumber = testcase.f16 ? 0x5140 : testcase.f32 ? 0x42280000 : 42;
  const expect = new Uint32Array([
  ...iterRange(128, (x) => {
    if (x * 4 === testcase.offset + offset) {
      return outMagicNumber;
    } else {
      return 0;
    }
  })]
  );
  t.expectGPUBufferValuesEqual(outputBuffer, expect);
}

/**
 * Performs a type-based read or write test for bufferView and bufferArrayView
 *
 * Note: This only covers types with no padding. Padding is covered by the layout tests.
 * @param read Test read if true
 * @param t The GPUTest
 * @param wgsl The shader code
 * @param eleTy The base element type
 * @param ty The test type
 * @param aspace The address space to test
 * @param offset The offset for the call
 * @param bufferSize The test buffer size
 */
export function runReadWriteTest(
read,
t,
wgsl,
eleTy,
ty,
aspace,
offset,
bufferSize)
{
  let num_eles = 1;
  if (ty instanceof VectorType) {
    num_eles *= ty.width;
  } else if (ty instanceof ArrayType) {
    num_eles *= ty.count;
  } else if (ty instanceof MatrixType) {
    num_eles *= ty.rows * ty.cols;
  }

  const bufferElements = bufferSize / eleTy.size;

  const start = offset / eleTy.size;
  const end = start + num_eles;
  const ins = [];
  const outs = [];
  for (let i = 0; i < bufferElements; i++) {
    if (i >= start && i < end) {
      ins.push(i + 4);
      outs.push(i + 4);
    } else {
      if (read) {
        ins.push(0);
      } else {
        outs.push(0);
      }
    }
  }
  if (!read && eleTy === Type.f16 && ins.length % 2 === 1) {
    ins.push(0);
  }
  if (read && eleTy === Type.f16 && outs.length % 2 === 1) {
    outs.push(0);
  }

  const inMap = {
    u32: typedArrayParam('Uint32Array', ins),
    i32: typedArrayParam('Int32Array', ins),
    f32: typedArrayParam('Float32Array', ins),
    f16: typedArrayParam('Float16Array', ins)
  };
  const inputData = typedArrayFromParam(
    inMap[eleTy.toString()]
  );

  const outMap = {
    u32: typedArrayParam('Uint32Array', outs),
    i32: typedArrayParam('Int32Array', outs),
    f32: typedArrayParam('Float32Array', outs),
    f16: typedArrayParam('Float16Array', outs)
  };
  const outputData = typedArrayFromParam(
    outMap[eleTy.toString()]
  );

  const inputBuffer = t.makeBufferWithContents(
    inputData,
    GPUBufferUsage.COPY_SRC | (
    aspace === 'uniform' ? GPUBufferUsage.UNIFORM : GPUBufferUsage.STORAGE)
  );
  const outputBuffer = t.createBufferTracked({
    size: outs.length * eleTy.size,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  const pipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: {
      module: t.device.createShaderModule({ code: wgsl })
    }
  });

  const bg = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
    {
      binding: 0,
      resource: {
        buffer: inputBuffer
      }
    },
    {
      binding: 1,
      resource: {
        buffer: outputBuffer
      }
    }]

  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bg);
  pass.dispatchWorkgroups(1, 1, 1);
  pass.end();
  t.queue.submit([encoder.finish()]);

  t.expectGPUBufferValuesEqual(outputBuffer, outputData);
}

/** @returns the bit pattern of the f32 value 'f' as an i32. */
export function f32Bits(f) {
  return new Int32Array(new Float32Array([f]).buffer)[0];
}

/** The scalar types that may be used by the lanes of a mixed type. */


/** @returns the size in bytes of the scalar type 's'. */
function mixedScalarSize(s) {
  return s === 'f16' ? 2 : 4;
}

/** A scalar lane of a mixed type. */









/**
 * A scalar, vector or structure type used by the mixed type aliasing tests.
 * A value of the type is treated as a flat list of scalar lanes. Structures may contain padding,
 * which is not part of any lane.
 */
















function scalar(elem) {
  const size = mixedScalarSize(elem);
  return { name: elem, size, align: size, lanes: [{ elem, offset: 0, access: '' }] };
}

function vec(n, elem) {
  const size = n * mixedScalarSize(elem);
  const lanes = [...Array(n).keys()].map((k) => ({
    elem,
    offset: k * mixedScalarSize(elem),
    access: `[${k}]`
  }));
  return { name: `vec${n}<${elem}>`, size, align: size, lanes };
}

/** @returns a structure type, laid out with the WGSL layout rules. */
function structType(name, members) {
  const roundUp = (n, k) => Math.ceil(n / k) * k;
  let end = 0;
  let align = 1;
  const laidOut = members.map(([memberName, type]) => {
    const offset = roundUp(end, type.align);
    end = offset + type.size;
    align = Math.max(align, type.align);
    return { name: memberName, type, offset };
  });
  const lanes = laidOut.flatMap((m) =>
  m.type.lanes.map((l) => ({
    elem: l.elem,
    offset: m.offset + l.offset,
    access: `.${m.name}${l.access}`
  }))
  );
  return { name, size: roundUp(end, align), align, lanes, struct: { members: laidOut } };
}

const kI32 = scalar('i32');
const kU32 = scalar('u32');
const kF32 = scalar('f32');
const kF16 = scalar('f16');
const kVec2I = vec(2, 'i32');
const kVec4I = vec(4, 'i32');
const kVec4U = vec(4, 'u32');
const kVec2F = vec(2, 'f32');
const kVec4F = vec(4, 'f32');
const kVec2H = vec(2, 'f16');
const kVec4H = vec(4, 'f16');
// Structures without padding.
const kStructI = structType('SI', [
['a', kI32],
['b', kI32],
['c', kI32],
['d', kI32]]
);
const kStructF = structType('SF', [
['a', kF32],
['b', kF32],
['c', kF32],
['d', kF32]]
);
const kStructV = structType('SV', [
['v', kVec2I],
['s', kI32],
['t', kI32]]
);
// Structures with padding.
/** 4 bytes of padding between 'a' and 'b'. */
const kPaddedI = structType('PI', [
['a', kI32],
['b', kVec2I]]
);
/** 4 bytes of padding between 'a' and 'b'. */
const kPaddedF = structType('PF', [
['a', kF32],
['b', kVec2F]]
);
/** 4 bytes of trailing padding. */
const kPaddedTrailingF = structType('PT', [
['v', kVec2F],
['s', kF32]]
);
/** 2 bytes of padding between 'a' and 'b', within the first 4-byte word. */
const kPaddedH = structType('PH', [
['a', kF16],
['b', kF32]]
);

/** @returns true if the type 't' has any f16 lanes. */
export function mixedTypeUsesF16(t) {
  return t.lanes.some((l) => l.elem === 'f16');
}

/** @returns the WGSL name of the type 't'. */
export function mixedTypeName(t) {
  return t.name;
}

/** @returns a WGSL constructor expression for a value of type 't', given an expression per lane. */
function mixedTypeCtorFromLanes(t, lanes) {
  if (t.struct) {
    let next = 0;
    const members = t.struct.members.map((m) => {
      const n = m.type.lanes.length;
      const expr = mixedTypeCtorFromLanes(m.type, lanes.slice(next, next + n));
      next += n;
      return expr;
    });
    return `${t.name}(${members.join(', ')})`;
  }
  return t.lanes.length === 1 ? lanes[0] : `${t.name}(${lanes.join(', ')})`;
}

/**
 * @returns a WGSL constructor expression for a value of type 't', where lane 'k' has the value
 * 'base + k', and 'base' is a WGSL scalar expression.
 */
function mixedTypeCtor(t, base) {
  const lanes = t.lanes.map(({ elem }, k) =>
  k === 0 ? `${elem}(${base})` : `${elem}(${base}) + ${elem}(${k})`
  );
  return mixedTypeCtorFromLanes(t, lanes);
}

/** @returns the WGSL declarations of the structures used by the types 'types', including nested. */
function mixedTypeStructDecls(types) {
  const decls = new Map();
  const visit = (t) => {
    if (!t.struct || decls.has(t.name)) {
      return;
    }
    t.struct.members.forEach((m) => visit(m.type));
    const members = t.struct.members.map((m) => `${m.name} : ${m.type.name}`).join(', ');
    decls.set(t.name, `struct ${t.name} { ${members} }`);
  };
  types.forEach(visit);
  return [...decls.values()].join('\n');
}

/**
 * @returns WGSL declarations required by the mixed type ops, where 'int' is the type of the loaded
 * view, and 'other' is the type of the stored view. This declares the structures used by the types
 * and the functions:
 *   'mixed_sub(a, b)' - lane-wise 'a - b'
 *   'mixed_xor(a, b)' - lane-wise 'a ^ b'
 * If either type uses f16, the shader must also 'enable f16;'.
 */
export function mixedTypeDecls(int, other) {
  const ty = mixedTypeName(int);
  const laneWise = (name, op) => {
    if (!int.struct) {
      return `fn ${name}(a : ${ty}, b : ${ty}) -> ${ty} { return a ${op} b; }`;
    }
    const lanes = int.lanes.map(({ access: l }) => `  r${l} = a${l} ${op} b${l};`);
    return `fn ${name}(a : ${ty}, b : ${ty}) -> ${ty} {\n  var r : ${ty};\n${lanes.join(
      '\n'
    )}\n  return r;\n}`;
  };
  return `${mixedTypeStructDecls([int, other])}
${laneWise('mixed_sub', '-')}
${laneWise('mixed_xor', '^')}
`;
}

/**
 * @returns WGSL statements that write each lane of the value 'value' of type 't', as an i32 bit
 * pattern, to consecutive elements of the i32 array 'array'. All lanes of 't' must be 32-bit.
 */
export function mixedTypeWriteLanes(t, value, array) {
  return t.lanes.
  map(({ access }, k) => `${array}[${k}] = bitcast<i32>(${value}${access});`).
  join('\n  ');
}

/**
 * The pairs of view types tested.
 * 'int' is a type with only i32 or u32 lanes, and at most 4 lanes. It is the only view that is
 * loaded from, which keeps the expected results exact: float values are never loaded, so denormal
 * flushing and NaN canonicalization cannot affect the results.
 * 'other' is a type that is only stored to.
 */
export const kMixedTypePairs = {
  // scalar - scalar
  i32_f32: { int: kI32, other: kF32 },
  u32_f32: { int: kU32, other: kF32 },
  i32_i32: { int: kI32, other: kI32 },
  // vector - scalar
  vec2i_f32: { int: kVec2I, other: kF32 },
  vec4i_f32: { int: kVec4I, other: kF32 },
  vec4u_f32: { int: kVec4U, other: kF32 },
  vec4u_u32: { int: kVec4U, other: kU32 },
  // scalar - vector
  i32_vec2f: { int: kI32, other: kVec2F },
  i32_vec4f: { int: kI32, other: kVec4F },
  i32_vec4u: { int: kI32, other: kVec4U },
  i32_vec4i: { int: kI32, other: kVec4I },
  // vector - vector
  vec2i_vec2f: { int: kVec2I, other: kVec2F },
  vec4i_vec4f: { int: kVec4I, other: kVec4F },
  vec4u_vec4f: { int: kVec4U, other: kVec4F },
  vec4i_vec2f: { int: kVec4I, other: kVec2F },
  vec2i_vec4f: { int: kVec2I, other: kVec4F },
  vec4i_vec4u: { int: kVec4I, other: kVec4U },
  vec4i_vec4i: { int: kVec4I, other: kVec4I },
  // struct - scalar
  structi_f32: { int: kStructI, other: kF32 },
  structv_f32: { int: kStructV, other: kF32 },
  i32_structf: { int: kI32, other: kStructF },
  // struct - vector
  structi_vec4f: { int: kStructI, other: kVec4F },
  structi_vec2f: { int: kStructI, other: kVec2F },
  vec4i_structf: { int: kVec4I, other: kStructF },
  // struct - struct
  structi_structf: { int: kStructI, other: kStructF },
  structv_structf: { int: kStructV, other: kStructF },
  // f16. The f16 stores only write part of a 4-byte word.
  i32_f16: { int: kI32, other: kF16 },
  u32_f16: { int: kU32, other: kF16 },
  i32_vec2h: { int: kI32, other: kVec2H },
  i32_vec4h: { int: kI32, other: kVec4H },
  vec4i_f16: { int: kVec4I, other: kF16 },
  vec2i_vec4h: { int: kVec2I, other: kVec4H },
  vec4i_vec4h: { int: kVec4I, other: kVec4H },
  // padded struct. Stores of padded structures must not write the padding.
  paddedi_f32: { int: kPaddedI, other: kF32 },
  paddedi_vec4f: { int: kPaddedI, other: kVec4F },
  i32_paddedf: { int: kI32, other: kPaddedF },
  vec4i_paddedf: { int: kVec4I, other: kPaddedF },
  i32_paddedtf: { int: kI32, other: kPaddedTrailingF },
  vec4i_paddedtf: { int: kVec4I, other: kPaddedTrailingF },
  paddedi_paddedf: { int: kPaddedI, other: kPaddedF },
  paddedi_structf: { int: kPaddedI, other: kStructF },
  structi_paddedf: { int: kStructI, other: kPaddedF },
  // padded struct with f16
  i32_paddedh: { int: kI32, other: kPaddedH },
  vec4i_paddedh: { int: kVec4I, other: kPaddedH },
  paddedi_paddedh: { int: kPaddedI, other: kPaddedH }
};

/**
 * How the two views overlap. Views always start on a 4-byte boundary.
 *  'same_start' - Both views start at the same byte.
 *  'offset'     - The views start at different bytes, and a lane of one view overlaps a lane of
 *                 the other. Not all pairs of types can do this while respecting alignment.
 *  'padding'    - The views overlap, but only lanes of one view overlap padding of the other.
 *                 Only possible for structures with padding.
 */
export const kMixedTypeOverlaps = ['same_start', 'offset', 'padding'];


/**
 * @returns the word offsets of the 'int' and 'other' views for the given overlap, or undefined if
 * the types cannot be positioned to satisfy the overlap. Word offsets are always at least 4.
 */
export function mixedTypeWords(
int,
other,
overlap)
{
  const lanesOverlap = (a, b) =>
  int.lanes.some((li) =>
  other.lanes.some((lo) => {
    const i0 = a * 4 + li.offset;
    const o0 = b * 4 + lo.offset;
    return i0 < o0 + mixedScalarSize(lo.elem) && o0 < i0 + mixedScalarSize(li.elem);
  })
  );
  const viewsOverlap = (a, b) =>
  a * 4 < b * 4 + other.size && b * 4 < a * 4 + int.size;
  for (let a = 4; a < 12; a++) {
    if (a * 4 % int.align !== 0) continue;
    for (let b = 4; b < 12; b++) {
      if (b * 4 % other.align !== 0) continue;
      let ok = false;
      switch (overlap) {
        case 'same_start':
          ok = a === b;
          break;
        case 'offset':
          ok = a !== b && lanesOverlap(a, b);
          break;
        case 'padding':
          ok = viewsOverlap(a, b) && !lanesOverlap(a, b);
          break;
      }
      if (ok) {
        return { intWord: a, otherWord: b };
      }
    }
  }
  return undefined;
}

/** A simulated memory of 4-byte words, used to calculate the expected results of mixed type tests. */





/** The runtime arguments passed to a mixed type op. */









/**
 * Stores a value of type 't' where lane 'k' is 'base + k', at word 'ptr'.
 * Only the bytes of the lanes are written. Padding is left untouched.
 */
function simStore(m, t, ptr, base) {
  t.lanes.forEach(({ elem, offset }, k) => {
    const v = base + k;
    const word = ptr + Math.floor(offset / 4);
    switch (elem) {
      case 'i32':
      case 'u32':
        m.st(word, v | 0);
        break;
      case 'f32':
        m.st(word, f32Bits(v));
        break;
      case 'f16':{
          const shift = offset % 4 * 8;
          const mask = 0xffff << shift;
          m.st(word, m.ld(word) & ~mask | float32ToFloat16Bits(v) << shift & mask);
          break;
        }
    }
  });
}

/** @returns the lanes of a value of type 't' at word 'ptr', as i32 bit patterns. */
function simLoad(m, t, ptr) {
  return t.lanes.map(({ elem, offset }) => {
    assert(elem === 'i32' || elem === 'u32', 'only integer views are loaded');
    return m.ld(ptr + offset / 4);
  });
}

/**
 * An operation on a pointer 'pi' to an integer type, and a pointer 'pf' to another type, that may
 * refer to overlapping memory. Shaders using these ops must include mixedTypeDecls(int, other).
 */
























/**
 * Operations that produce different results if an implementation assumes that differently typed
 * pointers do not alias (i.e. applies C/C++ style type-based alias analysis).
 */
export const kMixedTypeOps = {
  int_store_other_store_int_load: {
    wgsl: (int, other) => `
  *pi = ${mixedTypeCtor(int, 'va')};
  *pf = ${mixedTypeCtor(other, 'vf')};
  return *pi;`,
    sim: (m, int, other, pi, pf, { va, vf }) => {
      simStore(m, int, pi, va);
      simStore(m, other, pf, vf);
      return simLoad(m, int, pi);
    }
  },
  other_store_int_load: {
    wgsl: (int, other) => `
  *pf = ${mixedTypeCtor(other, 'vf')};
  return *pi;`,
    sim: (m, int, other, pi, pf, { vf }) => {
      simStore(m, other, pf, vf);
      return simLoad(m, int, pi);
    }
  },
  int_load_other_store_int_load: {
    wgsl: (int, other) => `
  let before = *pi;
  *pf = ${mixedTypeCtor(other, 'vf')};
  return mixed_sub(*pi, before);`,
    sim: (m, int, other, pi, pf, { vf }) => {
      const before = simLoad(m, int, pi);
      simStore(m, other, pf, vf);
      return simLoad(m, int, pi).map((v, k) => v - before[k] | 0);
    }
  },
  dead_other_store: {
    // The first store to '*pf' is only dead if '*pi' does not alias '*pf'.
    wgsl: (int, other) => `
  *pf = ${mixedTypeCtor(other, 'vf')};
  let tmp = *pi;
  *pf = ${mixedTypeCtor(other, 'va')};
  return tmp;`,
    sim: (m, int, other, pi, pf, { va, vf }) => {
      simStore(m, other, pf, vf);
      const tmp = simLoad(m, int, pi);
      simStore(m, other, pf, va);
      return tmp;
    }
  },
  loop_other_store_int_load: {
    // The load of '*pi' must not be hoisted out of the loop.
    wgsl: (int, other) => `
  var acc = ${mixedTypeName(int)}();
  for (var i = 0; i < n; i++) {
    *pf = ${mixedTypeCtor(other, 'i')};
    acc = mixed_xor(acc, *pi);
  }
  return acc;`,
    sim: (m, int, other, pi, pf, { n }) => {
      const acc = new Array(int.lanes.length).fill(0);
      for (let i = 0; i < n; i++) {
        simStore(m, other, pf, i);
        simLoad(m, int, pi).forEach((v, k) => {
          acc[k] ^= v;
        });
      }
      return acc;
    }
  }
};

/** The number of 4-byte words in each of the buffers 'A' and 'B' used by runMixedTypeAliasingTest. */
export const kMixedTypeBufferWords = 32;

/**
 * The runtime parameters passed to the shader of runMixedTypeAliasingTest in 'input.p'.
 *  p[0]: 'va'  - an i32 value to write.
 *  p[1]: 'vf'  - an f32 value to write, passed as an i32 and converted to f32.
 *  p[2]: 'n'   - a loop count.
 *  p[3]: 'idx' - a runtime value that views may use to form offsets and indices.
 */
export const kMixedTypeParams = [1000, 2000, 4, 1];

/** The value of 'input.p[3]', which may be used by views to form offsets and indices. */
export const kMixedTypeIdx = kMixedTypeParams[3];

/**
 * The kinds of buffer that the views are formed on:
 *  'workgroup'       - var<workgroup> of type buffer<N>
 *  'storage'         - var<storage, read_write> of type buffer<N>
 *  'storage_unsized' - var<storage, read_write> of type buffer
 */
export const kMixedTypeBuffers = ['workgroup', 'storage', 'storage_unsized'];



























/**
 * Runs a test where two differently typed pointers are formed from buffer views and used within a
 * single function. If 'aliased' is true, then both views refer to the same buffer, otherwise the
 * 'other' view refers to a different buffer.
 *
 * The shader initializes buffers 'A' and 'B' from 'input', calls the op, and writes the lanes of
 * the result of the op, followed by the contents of 'A' and 'B', to 'output'.
 */
export function runMixedTypeAliasingTest(t, params) {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const usesF16 = mixedTypeUsesF16(params.int) || mixedTypeUsesF16(params.other);
  if (usesF16) {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }

  const N = kMixedTypeBufferWords;
  let decls = '';
  switch (params.buffer) {
    case 'workgroup':
      decls = `var<workgroup> A : buffer<${N * 4}>;\nvar<workgroup> B : buffer<${N * 4}>;`;
      break;
    case 'storage':
      decls = `@group(0) @binding(2) var<storage, read_write> A : buffer<${N * 4}>;
@group(0) @binding(3) var<storage, read_write> B : buffer<${N * 4}>;`;
      break;
    case 'storage_unsized':
      decls = `@group(0) @binding(2) var<storage, read_write> A : buffer;
@group(0) @binding(3) var<storage, read_write> B : buffer;`;
      break;
  }

  const intTy = mixedTypeName(params.int);
  const writeResult = mixedTypeWriteLanes(params.int, 'r', 'output.r');

  const wgsl = `${usesF16 ? 'enable f16;' : ''}
struct In {
  a : array<i32, ${N}>,
  b : array<i32, ${N}>,
  p : array<i32, 4>,
}

struct Out {
  r : array<i32, 4>,
  a : array<i32, ${N}>,
  b : array<i32, ${N}>,
}

@group(0) @binding(0) var<storage, read> input : In;
@group(0) @binding(1) var<storage, read_write> output : Out;

${decls}

${mixedTypeDecls(params.int, params.other)}

fn f(va : i32, vf : f32, n : i32) -> ${intTy} {
  let pi = ${params.view(params.int, 'A', params.intWord)};
  let pf = ${params.view(params.other, params.aliased ? 'A' : 'B', params.otherWord)};
  ${params.op.wgsl(params.int, params.other)}
}

@compute @workgroup_size(1)
fn main() {
  *bufferView<array<i32, ${N}>>(&A, 0) = input.a;
  *bufferView<array<i32, ${N}>>(&B, 0) = input.b;
  let r = f(input.p[0], f32(input.p[1]), input.p[2]);
  ${writeResult}
  output.a = *bufferView<array<i32, ${N}>>(&A, 0);
  output.b = *bufferView<array<i32, ${N}>>(&B, 0);
}
`;

  // Simulate the op. 'A' is at word 0, and 'B' is at word N.
  const initA = Array.from({ length: N }, (_, i) => 10 + i);
  const initB = Array.from({ length: N }, (_, i) => 100 + i);
  const mem = [...initA, ...initB];
  const memory = {
    ld: (ptr) => mem[ptr],
    st: (ptr, value) => {
      mem[ptr] = value | 0;
    }
  };
  const [va, vf, n] = kMixedTypeParams;
  const pi = params.intWord;
  const pf = (params.aliased ? 0 : N) + params.otherWord;
  const r = params.op.sim(memory, params.int, params.other, pi, pf, { va, vf, n });
  const rOut = [0, 0, 0, 0];
  r.forEach((v, k) => {
    rOut[k] = v;
  });
  const expected = new Int32Array([...rOut, ...mem]);

  const pipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: { module: t.device.createShaderModule({ code: wgsl }) }
  });

  const inputBuffer = t.makeBufferWithContents(
    new Int32Array([...initA, ...initB, ...kMixedTypeParams]),
    GPUBufferUsage.STORAGE
  );
  const outputBuffer = t.createBufferTracked({
    size: expected.byteLength,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });
  const entries = [
  { binding: 0, resource: { buffer: inputBuffer } },
  { binding: 1, resource: { buffer: outputBuffer } }];

  if (params.buffer !== 'workgroup') {
    for (const binding of [2, 3]) {
      const buffer = t.createBufferTracked({ size: N * 4, usage: GPUBufferUsage.STORAGE });
      entries.push({ binding, resource: { buffer } });
    }
  }
  const bg = t.device.createBindGroup({ layout: pipeline.getBindGroupLayout(0), entries });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bg);
  pass.dispatchWorkgroups(1);
  pass.end();
  t.queue.submit([encoder.finish()]);

  t.expectGPUBufferValuesEqual(outputBuffer, expected);
}