/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Compute dispatch validation tests.
`;import { AllFeaturesMaxLimitsGPUTest } from '../.././gpu_test.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { keysOf } from '../../../common/util/data_tables.js';


export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);

g.test('dispatch,linear_indexing_range').
desc('Tests validation of total invocations for linear_indexing built-in values').
params((u) =>
u.
combine('builtin', ['global_invocation_index', 'workgroup_index']).
beginSubcases().
combine('size', ['max', 'valid'])
).
fn((t) => {
  // Other builtins are not tested due to onerous runtimes.
  t.skipIf(!t.hasLanguageFeature('linear_indexing'), 'Missing linear_indexing language feature');

  // Spec limits:
  // - maxComputeWorkgroupsPerDimension = 65535
  const { maxComputeWorkgroupsPerDimension } = t.device.limits;
  const x = t.params.builtin === 'global_invocation_index' ? 2 : 1,
    y = 1,
    z = 1;
  const wgSize = x * y * z;
  const countX = maxComputeWorkgroupsPerDimension;
  const countY = t.params.size === 'max' ? maxComputeWorkgroupsPerDimension : 1;
  const countZ = t.params.builtin === 'workgroup_index' ? 2 : 1;

  const totalInvocations = wgSize * countX * countY * countZ;
  t.skipIf(t.params.size === 'max' && totalInvocations <= 0xffffffff, 'Uninteresting test');

  const code = `
@compute @workgroup_size(${x}, ${y}, ${z})
fn main(@builtin(${t.params.builtin}) input : u32) {
  _ = input;
}`;

  const shaderModule = t.device.createShaderModule({ code });
  const computePipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: {
      module: shaderModule
    }
  });
  const commandEncoder = t.device.createCommandEncoder();
  const computePassEncoder = commandEncoder.beginComputePass();
  computePassEncoder.setPipeline(computePipeline);
  computePassEncoder.dispatchWorkgroups(countX, countY, countZ);
  computePassEncoder.end();

  t.expectValidationError(() => {
    commandEncoder.finish();
  }, t.params.size === 'max');
});

g.test('dispatchIndirect,linear_indexing_range').
desc('Tests dispatchIndirect skips when linear_indexing is out of range').
params((u) =>
u.
combine('builtin', ['global_invocation_index', 'workgroup_index']).
beginSubcases().
combine('size', ['max', 'valid'])
).
fn((t) => {
  // Other builtins are not tested due to onerous runtimes.
  t.skipIf(!t.hasLanguageFeature('linear_indexing'), 'Missing linear_indexing language feature');

  // Spec limits:
  // - maxComputeWorkgroupsPerDimension = 65535
  const { maxComputeWorkgroupsPerDimension } = t.device.limits;
  const x = t.params.builtin === 'global_invocation_index' ? 2 : 1,
    y = 1,
    z = 1;
  const wgSize = x * y * z;
  const countX = maxComputeWorkgroupsPerDimension;
  const countY = t.params.size === 'max' ? maxComputeWorkgroupsPerDimension : 1;
  const countZ = t.params.builtin === 'workgroup_index' ? 2 : 1;

  const totalInvocations = wgSize * countX * countY * countZ;
  t.skipIf(t.params.size === 'max' && totalInvocations <= 0xffffffff, 'Uninteresting test');

  const kMagic = 0xdeadbeef;
  const code = `
@group(0) @binding(0)
var<storage, read_write> out : u32;

@compute @workgroup_size(${x}, ${y}, ${z})
fn main(@builtin(${t.params.builtin}) input : u32,
        @builtin(global_invocation_id) gid : vec3u) {
  _ = input;
  if (gid.x == 0 && gid.y == 0 && gid.z == 0) {
    out = ${kMagic};
  }
}`;

  const dispatchIndirectCounts = new Uint32Array(3);
  dispatchIndirectCounts[0] = countX;
  dispatchIndirectCounts[1] = countY;
  dispatchIndirectCounts[2] = countZ;
  const indirectBuffer = t.makeBufferWithContents(
    dispatchIndirectCounts,
    GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST | GPUBufferUsage.INDIRECT
  );
  t.trackForCleanup(indirectBuffer);
  const outputBuffer = t.makeBufferWithContents(
    new Uint32Array([0]),
    GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE
  );
  t.trackForCleanup(outputBuffer);

  const shaderModule = t.device.createShaderModule({ code });
  const computePipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: {
      module: shaderModule
    }
  });
  const bg = t.device.createBindGroup({
    layout: computePipeline.getBindGroupLayout(0),
    entries: [
    {
      binding: 0,
      resource: {
        buffer: outputBuffer
      }
    }]

  });
  const commandEncoder = t.device.createCommandEncoder();
  const computePassEncoder = commandEncoder.beginComputePass();
  computePassEncoder.setPipeline(computePipeline);
  computePassEncoder.setBindGroup(0, bg);
  computePassEncoder.dispatchWorkgroupsIndirect(indirectBuffer, 0);
  computePassEncoder.end();
  t.queue.submit([commandEncoder.finish()]);

  const expected = t.params.size === 'max' ? 0 : kMagic;
  t.expectGPUBufferValuesEqual(outputBuffer, new Uint32Array([expected]));
});








const kRequiredSizeCases = {
  ro_storage_32bytes: {
    code: `
@group(0) @binding(0) var<storage> v : array<vec4u, 2>;
@compute @workgroup_size(1)
fn main() {
  _ = v;
}`,
    size: 32,
    binding_type: 'read-only-storage'
  },
  storage_32bytes: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : array<vec4u, 2>;
@compute @workgroup_size(1)
fn main() {
  _ = v;
}`,
    size: 32,
    binding_type: 'storage'
  },
  uniform_32bytes: {
    code: `
@group(0) @binding(0) var<uniform> v : array<vec4u, 2>;
@compute @workgroup_size(1)
fn main() {
  _ = v;
}`,
    size: 32,
    binding_type: 'uniform'
  },
  ro_storage_sized_buffer: {
    code: `
@group(0) @binding(0) var<storage> v : buffer<32>;
@compute @workgroup_size(1)
fn main() {
  _ = &v;
}`,
    size: 32,
    binding_type: 'read-only-storage',
    requires: ['buffer_view']
  },
  storage_sized_buffer: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer<32>;
@compute @workgroup_size(1)
fn main() {
  _ = &v;
}`,
    size: 32,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  uniform_sized_buffer: {
    code: `
@group(0) @binding(0) var<uniform> v : buffer<32>;
@compute @workgroup_size(1)
fn main() {
  _ = &v;
}`,
    size: 32,
    binding_type: 'uniform',
    requires: ['buffer_view']
  },
  ro_storage_unsized_buffer_bufferView1: {
    code: `
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<array<u32, 16>>(&v, 0);
}`,
    size: 16 * 4,
    binding_type: 'read-only-storage',
    requires: ['buffer_view']
  },
  ro_storage_unsized_buffer_bufferView2: {
    code: `
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<array<u32, 16>>(&v, 56);
}`,
    // Offset does not count toward min binding size.
    size: 16 * 4,
    binding_type: 'read-only-storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferView3: {
    code: `
struct S {
  a: vec4u,
  b: vec2u,
}
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let x = 16u;
  let p = bufferView<S>(&v, x);
}`,
    size: 32,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferView4: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<array<vec2u>>(&v, 0);
}`,
    size: 8,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferView5: {
    code: `
struct S {
  a: array<u32, 4>
}

@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<vec2u>(&v, 0);
  let q = bufferView<S>(&v, 0);
}`,
    size: 16,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferView6: {
    code: `
struct S {
  a: array<u32, 4>
}

@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<u32>(&v, 16);
  let q = bufferView<S>(&v, 0);
}`,
    // Offset does not count toward min binding size.
    size: 16,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferView7: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<vec2i>(&v, 16);
}
@fragment
fn main2() {
  let q = bufferView<vec4u>(&v, 32);
}`,
    // Offset does not count toward min binding size.
    size: 8,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferView8: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<vec2f>(&v, 16);
}
fn foo() {
  let q = bufferView<vec4u>(&v, 32);
}`,
    // Offset does not count toward min binding size.
    size: 8,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferView9: {
    code: `
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferView<u32>(&v, 16);
  foo(&v);
}
fn foo(p : ptr<storage, buffer, read_write>) {
  let q = bufferView<vec4u>(&v, 32);
}`,
    // Offset does not count toward min binding size.
    size: 16,
    binding_type: 'storage',
    requires: ['buffer_view', 'unrestricted_pointer_parameters']
  },
  ro_storage_unsized_buffer_bufferArrayView1: {
    code: `
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferArrayView<array<vec2u>>(&v, 0, 32);
}`,
    // Size does not count toward min binding size.
    size: 8,
    binding_type: 'read-only-storage',
    requires: ['buffer_view']
  },
  ro_storage_unsized_buffer_bufferArrayView2: {
    code: `
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferArrayView<array<vec2f>>(&v, 16, 32);
}`,
    // Offset and size do not count toward min binding size.
    size: 8,
    binding_type: 'read-only-storage',
    requires: ['buffer_view']
  },
  ro_storage_unsized_buffer_bufferArrayView3: {
    code: `
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let o = 16;
  let s = 32;
  let p = bufferArrayView<array<vec2u>>(&v, o, s);
}`,
    size: 8,
    binding_type: 'read-only-storage',
    requires: ['buffer_view']
  },
  ro_storage_unsized_buffer_bufferArrayView4: {
    code: `
struct S {
  a: vec4u,
  b: u32,
}
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let o = 16;
  let s = 32;
  let p = bufferArrayView<array<S>>(&v, o, s);
}`,
    size: 32,
    binding_type: 'read-only-storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferArrayView5: {
    code: `
struct S {
  a : vec4f,
  b : vec4f,
}
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let o = 0u;
  let s = 64;
  let p1 = bufferArrayView<array<u32>>(&v, o, s);
  let p2 = bufferArrayView<array<S>>(&v, o, s);
}`,
    size: 32,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferArrayView6: {
    code: `
struct S {
  a : vec4f,
  b : vec4f,
}
var<private> o : i32;
var<private> s : i32;
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferArrayView<array<vec4u>>(&v, o, s);
}
fn foo() {
  let p = bufferArrayView<array<S>>(&v, o, s);
}`,
    size: 16,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferArrayView7: {
    code: `
struct S {
  a : vec4f,
  b : vec4f,
}
var<private> o : i32;
var<private> s : i32;
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let p = bufferArrayView<array<vec4u>>(&v, o, s);
}
@fragment
fn foo() {
  let p = bufferArrayView<array<S>>(&v, o, s);
}`,
    size: 16,
    binding_type: 'storage',
    requires: ['buffer_view']
  },
  storage_unsized_buffer_bufferArrayView8: {
    code: `
struct S {
  a : vec4f,
  b : vec4f,
}
var<private> o : i32;
var<private> s : i32;
@group(0) @binding(0) var<storage, read_write> v : buffer;
@compute @workgroup_size(1)
fn main() {
  bar();
  foo();
}
fn bar() {
  let p = bufferArrayView<array<vec4u>>(&v, o, s);
}
fn foo() {
  let p = bufferArrayView<array<S>>(&v, o, s);
}`,
    size: 32,
    binding_type: 'storage',
    requires: ['buffer_view']
  }
};

g.test('shader_required_buffer_size').
desc('Test that dispatch time validation occurs about the required buffer size').
params((u) =>
u.
combine('case', keysOf(kRequiredSizeCases)).
beginSubcases().
combine('valid', [false, true]).
combine('layout', ['auto', 'explicit'])
).
fn((t) => {
  const testcase = kRequiredSizeCases[t.params.case];
  const features = testcase.requires ?? [];
  features.forEach((f) => {
    t.skipIfLanguageFeatureNotSupported(f);
  });

  const buffer = t.createBufferTracked({
    size: t.params.valid ? testcase.size : testcase.size - 4,
    usage: testcase.binding_type === 'uniform' ? GPUBufferUsage.UNIFORM : GPUBufferUsage.STORAGE
  });

  const bgLayout = t.device.createBindGroupLayout({
    entries: [
    {
      binding: 0,
      visibility: GPUShaderStage.COMPUTE,
      buffer: {
        type: testcase.binding_type,
        minBindingSize: 0
      }
    }]

  });
  const layout = t.device.createPipelineLayout({ bindGroupLayouts: [bgLayout] });

  const pipeline = t.device.createComputePipeline({
    layout: t.params.layout === 'auto' ? 'auto' : layout,
    compute: {
      module: t.device.createShaderModule({ code: testcase.code })
    }
  });

  if (t.params.layout === 'auto' && !t.params.valid) {
    // 'auto' layout get minBindingSize from the shader.
    t.expectValidationError(() => {
      t.device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
        {
          binding: 0,
          resource: {
            buffer
          }
        }]

      }),
      true;
    });
  } else {
    // Expect dispatch time validation.
    const bg = t.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
      {
        binding: 0,
        resource: {
          buffer
        }
      }]

    });

    const commandEncoder = t.device.createCommandEncoder();
    const computePassEncoder = commandEncoder.beginComputePass();
    computePassEncoder.setPipeline(pipeline);
    computePassEncoder.setBindGroup(0, bg);
    computePassEncoder.dispatchWorkgroups(1);
    computePassEncoder.end();

    t.expectValidationError(() => {
      commandEncoder.finish();
    }, !t.params.valid);
  }
});