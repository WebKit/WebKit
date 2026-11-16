/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Execution tests for bufferLength

Tests sized and unsized buffers across all address spaces (where applicable).
Tested against buffers with static and dynamic offsets.
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('sized_buffer').
desc('Tests bufferLength directly on sized buffer variables').
params((u) =>
u.
combine('size', [256, 512, 600, 1024, 2048]).
beginSubcases().
combine('param', ['none', 'unsized', 'sized']).
combine('padding', [false, true]).
combine('dynamic_offset', [false, true]).
combine('offset', [0, 256])
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  if (t.params.param !== 'none') {
    t.skipIfLanguageFeatureNotSupported('unrestricted_pointer_parameters');
  }

  let fn_decls = '';
  const assigns = ['', '', '', ''];
  switch (t.params.param) {
    case 'none':
      assigns[0] = 'bufferLength(&uniform_buffer)';
      assigns[1] = 'bufferLength(&ro_storage_buffer)';
      assigns[2] = 'bufferLength(&storage_buffer)';
      assigns[3] = 'bufferLength(&workgroup_buffer)';
      break;
    case 'unsized':
      fn_decls = `
fn uniformUnsized(p : ptr<uniform, buffer>) -> u32 {
  return bufferLength(p);
}
fn roStorageUnsized(p : ptr<storage, buffer>) -> u32 {
  return bufferLength(p);
}
fn storageUnsized(p : ptr<storage, buffer, read_write>) -> u32 {
  return bufferLength(p);
}
fn workgroupUnsized(p : ptr<workgroup, buffer>) -> u32 {
  return bufferLength(p);
}`;
      assigns[0] = 'uniformUnsized(&uniform_buffer)';
      assigns[1] = 'roStorageUnsized(&ro_storage_buffer)';
      assigns[2] = 'storageUnsized(&storage_buffer)';
      assigns[3] = 'workgroupUnsized(&workgroup_buffer)';
      break;
    case 'sized':
      fn_decls = `
fn uniformSized(p : ptr<uniform, buffer<${t.params.size / 2}>>) -> u32 {
  return bufferLength(p);
}
fn roStorageSized(p : ptr<storage, buffer<${t.params.size / 2}>>) -> u32 {
  return bufferLength(p);
}
fn storageSized(p : ptr<storage, buffer<${t.params.size / 2}>, read_write>) -> u32 {
  return bufferLength(p);
}
fn workgroupSized(p : ptr<workgroup, buffer<${t.params.size / 2}>>) -> u32 {
  return bufferLength(p);
}`;
      assigns[0] = 'uniformSized(&uniform_buffer)';
      assigns[1] = 'roStorageSized(&ro_storage_buffer)';
      assigns[2] = 'storageSized(&storage_buffer)';
      assigns[3] = 'workgroupSized(&workgroup_buffer)';
      break;
  }

  const wgsl = `
@group(0) @binding(0) var<uniform> uniform_buffer : buffer<${t.params.size}>;
@group(0) @binding(1) var<storage> ro_storage_buffer : buffer<${t.params.size}>;
@group(0) @binding(2) var<storage, read_write> storage_buffer : buffer<${t.params.size}>;
var<workgroup> workgroup_buffer : buffer<${t.params.size}>;

@group(0) @binding(3) var<storage, read_write> out : array<u32, 4>;

${fn_decls}

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) lid : u32) {
  out[0] = ${assigns[0]};
  out[1] = ${assigns[1]};
  out[2] = ${assigns[2]};
  out[3] = ${assigns[3]};
}`;

  const padding = t.params.padding ? 256 : 0;
  const offset = t.params.offset;
  const dynOffset = t.params.dynamic_offset ? 256 : 0;
  const bufferSize = t.params.size + padding + offset + dynOffset;
  const noOffsetSize = t.params.size + padding;

  const uniformBuffer = t.createBufferTracked({
    size: bufferSize,
    usage: GPUBufferUsage.UNIFORM
  });
  const roStorageBuffer = t.createBufferTracked({
    size: bufferSize,
    usage: GPUBufferUsage.STORAGE
  });
  const storageBuffer = t.createBufferTracked({
    size: bufferSize,
    usage: GPUBufferUsage.STORAGE
  });
  const outputBuffer = t.createBufferTracked({
    size: 4 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  const bgLayout = t.device.createBindGroupLayout({
    entries: [
    {
      binding: 0,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'uniform',
        hasDynamicOffset: t.params.dynamic_offset,
        minBindingSize: t.params.size
      }
    },
    {
      binding: 1,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'read-only-storage',
        hasDynamicOffset: t.params.dynamic_offset,
        minBindingSize: t.params.size
      }
    },
    {
      binding: 2,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'storage',
        hasDynamicOffset: t.params.dynamic_offset,
        minBindingSize: t.params.size
      }
    },
    {
      binding: 3,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'storage',
        minBindingSize: 4 * 4
      }
    }]

  });

  const pipelineLayout = t.device.createPipelineLayout({
    bindGroupLayouts: [bgLayout]
  });
  const pipeline = t.device.createComputePipeline({
    layout: pipelineLayout,
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
        buffer: uniformBuffer,
        offset,
        size: noOffsetSize
      }
    },
    {
      binding: 1,
      resource: {
        buffer: roStorageBuffer,
        offset,
        size: noOffsetSize
      }
    },
    {
      binding: 2,
      resource: {
        buffer: storageBuffer,
        offset,
        size: noOffsetSize
      }
    },
    {
      binding: 3,
      resource: {
        buffer: outputBuffer,
        size: 4 * 4
      }
    }]

  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  if (t.params.dynamic_offset) {
    pass.setBindGroup(0, bg, [dynOffset, dynOffset, dynOffset]);
  } else {
    pass.setBindGroup(0, bg);
  }
  pass.dispatchWorkgroups(1, 1, 1);
  pass.end();
  t.queue.submit([encoder.finish()]);

  const expectedSize = t.params.param === 'sized' ? t.params.size / 2 : t.params.size;
  const expected = new Uint32Array([expectedSize, expectedSize, expectedSize, expectedSize]);
  t.expectGPUBufferValuesEqual(outputBuffer, expected);
});

g.test('unsized_buffer').
desc('Tests bufferLength directly on unsized buffer variables').
params((u) =>
u.
combine('size', [256, 512, 600, 1024, 2048]).
beginSubcases().
combine('param', ['none', 'unsized']).
combine('padding', [false, true]).
combine('dynamic_offset', [false, true]).
combine('offset', [0, 256])
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  if (t.params.param !== 'none') {
    t.skipIfLanguageFeatureNotSupported('unrestricted_pointer_parameters');
  }

  let fn_decls = '';
  const assigns = ['', '', '', ''];
  switch (t.params.param) {
    case 'none':
      assigns[0] = 'bufferLength(&ro_storage_buffer)';
      assigns[1] = 'bufferLength(&storage_buffer)';
      break;
    case 'unsized':
      fn_decls = `
fn roStorageUnsized(p : ptr<storage, buffer>) -> u32 {
  return bufferLength(p);
}
fn storageUnsized(p : ptr<storage, buffer, read_write>) -> u32 {
  return bufferLength(p);
}`;
      assigns[0] = 'roStorageUnsized(&ro_storage_buffer)';
      assigns[1] = 'storageUnsized(&storage_buffer)';
      break;
  }

  const wgsl = `
@group(0) @binding(0) var<storage> ro_storage_buffer : buffer;
@group(0) @binding(1) var<storage, read_write> storage_buffer : buffer;

@group(0) @binding(2) var<storage, read_write> out : array<u32, 4>;

${fn_decls}

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) lid : u32) {
  out[0] = ${assigns[0]};
  out[1] = ${assigns[1]};
}`;

  const padding = t.params.padding ? 256 : 0;
  const offset = t.params.offset;
  const dynOffset = t.params.dynamic_offset ? 256 : 0;
  const bufferSize = t.params.size + padding + offset + dynOffset;
  const noOffsetSize = t.params.size + padding;

  const roStorageBuffer = t.createBufferTracked({
    size: bufferSize,
    usage: GPUBufferUsage.STORAGE
  });
  const storageBuffer = t.createBufferTracked({
    size: bufferSize,
    usage: GPUBufferUsage.STORAGE
  });
  const outputBuffer = t.createBufferTracked({
    size: 4 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  const bgLayout = t.device.createBindGroupLayout({
    entries: [
    {
      binding: 0,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'read-only-storage',
        hasDynamicOffset: t.params.dynamic_offset,
        minBindingSize: t.params.size
      }
    },
    {
      binding: 1,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'storage',
        hasDynamicOffset: t.params.dynamic_offset,
        minBindingSize: t.params.size
      }
    },
    {
      binding: 2,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: 'storage',
        minBindingSize: 4 * 4
      }
    }]

  });

  const pipelineLayout = t.device.createPipelineLayout({
    bindGroupLayouts: [bgLayout]
  });
  const pipeline = t.device.createComputePipeline({
    layout: pipelineLayout,
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
        buffer: roStorageBuffer,
        offset,
        size: noOffsetSize
      }
    },
    {
      binding: 1,
      resource: {
        buffer: storageBuffer,
        offset,
        size: noOffsetSize
      }
    },
    {
      binding: 2,
      resource: {
        buffer: outputBuffer,
        size: 4 * 4
      }
    }]

  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  if (t.params.dynamic_offset) {
    pass.setBindGroup(0, bg, [dynOffset, dynOffset]);
  } else {
    pass.setBindGroup(0, bg);
  }
  pass.dispatchWorkgroups(1, 1, 1);
  pass.end();
  t.queue.submit([encoder.finish()]);

  const expected = new Uint32Array([noOffsetSize, noOffsetSize]);
  t.expectGPUBufferValuesEqual(outputBuffer, expected);
});

g.test('max_size_buffer').
desc('Test with a maximum sized buffer').
params((u) =>
u.
combine('aspace', ['workgroup', 'uniform', 'storage', 'ro_storage']).
combine('sized', [false, true]).
filter((t) => {
  return t.sized === true || t.aspace === 'storage' || t.aspace === 'ro_storage';
})
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');

  const {
    maxUniformBufferBindingSize,
    maxStorageBufferBindingSize,
    maxComputeWorkgroupStorageSize
  } = t.device.limits;

  let size = 16;
  switch (t.params.aspace) {
    case 'workgroup':
      size = maxComputeWorkgroupStorageSize;
      break;
    case 'uniform':
      size = maxUniformBufferBindingSize;
      break;
    case 'storage':
    case 'ro_storage':
      size = maxStorageBufferBindingSize;
      break;
  }

  const type = `buffer${t.params.sized ? `<${size}u>` : ``}`;
  let decl = '';
  switch (t.params.aspace) {
    case 'workgroup':
      decl = `var<workgroup> v : ${type};\n@group(0) @binding(1) var<storage> dummy : u32;`;
      break;
    case 'uniform':
      decl = `@group(0) @binding(1) var<uniform> v: ${type};`;
      break;
    case 'storage':
      decl = `@group(0) @binding(1) var<storage, read_write> v : ${type};`;
      break;
    case 'ro_storage':
      decl = `@group(0) @binding(1) var<storage> v : ${type};`;
      break;
  }

  const wgsl = `
@group(0) @binding(0) var<storage, read_write> out : u32;
${decl}

@compute @workgroup_size(1)
fn main() {
  out = bufferLength(&v);
  ${t.params.aspace === 'workgroup' ? '_ = dummy;' : ''}
}`;

  const outputBuffer = t.createBufferTracked({
    size: 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });
  // For the workgroup case we create a dummy storage buffer to simplify bindings.
  const inputBuffer = t.createBufferTracked({
    size,
    usage: t.params.aspace === 'uniform' ? GPUBufferUsage.UNIFORM : GPUBufferUsage.STORAGE
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
        buffer: outputBuffer
      }
    },
    {
      binding: 1,
      resource: {
        buffer: inputBuffer
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

  const expected = new Uint32Array([size]);
  t.expectGPUBufferValuesEqual(outputBuffer, expected);
});