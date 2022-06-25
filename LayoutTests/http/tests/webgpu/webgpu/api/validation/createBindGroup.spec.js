/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
  createBindGroup validation tests.

  TODO: Ensure sure tests cover all createBindGroup validation rules.
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { assert, unreachable } from '../../../common/util/util.js';
import {
  allBindingEntries,
  bindingTypeInfo,
  bufferBindingEntries,
  bufferBindingTypeInfo,
  kBindableResources,
  kTextureUsages,
  kTextureViewDimensions,
  sampledAndStorageBindingEntries,
  texBindingTypeInfo,
} from '../../capability_info.js';
import { GPUConst } from '../../constants.js';
import { kResourceStates } from '../../gpu_test.js';
import { getTextureDimensionFromView } from '../../util/texture/base.js';

import { ValidationTest } from './validation_test.js';

function clone(descriptor) {
  return JSON.parse(JSON.stringify(descriptor));
}

export const g = makeTestGroup(ValidationTest);

g.test('binding_count_mismatch')
  .desc('Test that the number of entries must match the number of entries in the BindGroupLayout.')
  .paramsSubcasesOnly(u =>
    u //
      .combine('layoutEntryCount', [1, 2, 3])
      .combine('bindGroupEntryCount', [1, 2, 3])
  )
  .fn(async t => {
    const { layoutEntryCount, bindGroupEntryCount } = t.params;

    const layoutEntries = [];
    for (let i = 0; i < layoutEntryCount; ++i) {
      layoutEntries.push({
        binding: i,
        visibility: GPUShaderStage.COMPUTE,
        buffer: { type: 'storage' },
      });
    }
    const bindGroupLayout = t.device.createBindGroupLayout({ entries: layoutEntries });

    const entries = [];
    for (let i = 0; i < bindGroupEntryCount; ++i) {
      entries.push({
        binding: i,
        resource: { buffer: t.getStorageBuffer() },
      });
    }

    const shouldError = layoutEntryCount !== bindGroupEntryCount;
    t.expectValidationError(() => {
      t.device.createBindGroup({
        entries,
        layout: bindGroupLayout,
      });
    }, shouldError);
  });

g.test('binding_must_be_present_in_layout')
  .desc(
    'Test that the binding slot for each entry matches a binding slot defined in the BindGroupLayout.'
  )
  .paramsSubcasesOnly(u =>
    u //
      .combine('layoutBinding', [0, 1, 2])
      .combine('binding', [0, 1, 2])
  )
  .fn(async t => {
    const { layoutBinding, binding } = t.params;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        { binding: layoutBinding, visibility: GPUShaderStage.COMPUTE, buffer: { type: 'storage' } },
      ],
    });

    const descriptor = {
      entries: [{ binding, resource: { buffer: t.getStorageBuffer() } }],
      layout: bindGroupLayout,
    };

    const shouldError = layoutBinding !== binding;
    t.expectValidationError(() => {
      t.device.createBindGroup(descriptor);
    }, shouldError);
  });

g.test('binding_must_contain_resource_defined_in_layout')
  .desc(
    'Test that only compatible resource types specified in the BindGroupLayout are allowed for each entry.'
  )
  .paramsSubcasesOnly(u =>
    u //
      .combine('resourceType', kBindableResources)
      .combine('entry', allBindingEntries(false))
  )
  .fn(t => {
    const { resourceType, entry } = t.params;
    const info = bindingTypeInfo(entry);

    const layout = t.device.createBindGroupLayout({
      entries: [{ binding: 0, visibility: GPUShaderStage.COMPUTE, ...entry }],
    });

    const resource = t.getBindingResource(resourceType);

    let resourceBindingIsCompatible;
    switch (info.resource) {
      // Either type of sampler may be bound to a filtering sampler binding.
      case 'filtSamp':
        resourceBindingIsCompatible = resourceType === 'filtSamp' || resourceType === 'nonFiltSamp';
        break;
      // But only non-filtering samplers can be used with non-filtering sampler bindings.
      case 'nonFiltSamp':
        resourceBindingIsCompatible = resourceType === 'nonFiltSamp';
        break;
      default:
        resourceBindingIsCompatible = info.resource === resourceType;
        break;
    }

    t.expectValidationError(() => {
      t.device.createBindGroup({ layout, entries: [{ binding: 0, resource }] });
    }, !resourceBindingIsCompatible);
  });

g.test('texture_binding_must_have_correct_usage')
  .desc('Tests that texture bindings must have the correct usage.')
  .paramsSubcasesOnly(u =>
    u //
      .combine('entry', sampledAndStorageBindingEntries(false))
      .combine('usage', kTextureUsages)
      .unless(({ entry, usage }) => {
        const info = texBindingTypeInfo(entry);
        // Can't create the texture for this (usage=STORAGE_BINDING and sampleCount=4), so skip.
        return usage === GPUConst.TextureUsage.STORAGE_BINDING && info.resource === 'sampledTexMS';
      })
  )
  .fn(async t => {
    const { entry, usage } = t.params;
    const info = texBindingTypeInfo(entry);

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [{ binding: 0, visibility: GPUShaderStage.FRAGMENT, ...entry }],
    });

    const descriptor = {
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage,
      sampleCount: info.resource === 'sampledTexMS' ? 4 : 1,
    };

    const resource = t.device.createTexture(descriptor).createView();

    const shouldError = usage !== info.usage;
    t.expectValidationError(() => {
      t.device.createBindGroup({
        entries: [{ binding: 0, resource }],
        layout: bindGroupLayout,
      });
    }, shouldError);
  });

g.test('texture_must_have_correct_component_type')
  .desc(
    `
    Tests that texture bindings must have a format that matches the sample type specified in the BindGroupLayout.
    - Tests a compatible format for every sample type
    - Tests an incompatible format for every sample type`
  )
  .params(u => u.combine('sampleType', ['float', 'sint', 'uint']))
  .fn(async t => {
    const { sampleType } = t.params;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.FRAGMENT,
          texture: { sampleType },
        },
      ],
    });

    let format;
    if (sampleType === 'float') {
      format = 'r8unorm';
    } else if (sampleType === 'sint') {
      format = 'r8sint';
    } else if (sampleType === 'uint') {
      format = 'r8uint';
    } else {
      unreachable('Unexpected texture component type');
    }

    const goodDescriptor = {
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.TEXTURE_BINDING,
    };

    // Control case
    t.device.createBindGroup({
      entries: [
        {
          binding: 0,
          resource: t.device.createTexture(goodDescriptor).createView(),
        },
      ],

      layout: bindGroupLayout,
    });

    function* mismatchedTextureFormats() {
      if (sampleType !== 'float') {
        yield 'r8unorm';
      }
      if (sampleType !== 'sint') {
        yield 'r8sint';
      }
      if (sampleType !== 'uint') {
        yield 'r8uint';
      }
    }

    // Mismatched texture binding formats are not valid.
    for (const mismatchedTextureFormat of mismatchedTextureFormats()) {
      const badDescriptor = clone(goodDescriptor);
      badDescriptor.format = mismatchedTextureFormat;

      t.expectValidationError(() => {
        t.device.createBindGroup({
          entries: [{ binding: 0, resource: t.device.createTexture(badDescriptor).createView() }],
          layout: bindGroupLayout,
        });
      });
    }
  });

g.test('texture_must_have_correct_dimension')
  .desc(
    `
    Test that bound texture views match the dimensions supplied in the BindGroupLayout
    - Test for every GPUTextureViewDimension`
  )
  .params(u =>
    u
      .combine('viewDimension', kTextureViewDimensions)
      .beginSubcases()
      .combine('dimension', kTextureViewDimensions)
  )
  .fn(async t => {
    const { viewDimension, dimension } = t.params;
    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.FRAGMENT,
          texture: { viewDimension },
        },
      ],
    });

    let height = 16;
    let depthOrArrayLayers = 6;
    if (dimension === '1d') {
      height = 1;
      depthOrArrayLayers = 1;
    }

    const texture = t.device.createTexture({
      size: { width: 16, height, depthOrArrayLayers },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.TEXTURE_BINDING,
      dimension: getTextureDimensionFromView(dimension),
    });

    const shouldError = viewDimension !== dimension;
    const textureView = texture.createView({ dimension });

    t.expectValidationError(() => {
      t.device.createBindGroup({
        entries: [{ binding: 0, resource: textureView }],
        layout: bindGroupLayout,
      });
    }, shouldError);
  });

g.test('buffer_offset_and_size_for_bind_groups_match')
  .desc(
    `
    Test that a buffer binding's [offset, offset + size) must be contained in the BindGroup entry's buffer.
    - Test for various offsets and sizes`
  )
  .paramsSubcasesOnly([
    { offset: 0, size: 512, _success: true }, // offset 0 is valid
    { offset: 256, size: 256, _success: true }, // offset 256 (aligned) is valid

    // Touching the end of the buffer
    { offset: 0, size: 1024, _success: true },
    { offset: 0, size: undefined, _success: true },
    { offset: 256 * 3, size: 256, _success: true },
    { offset: 256 * 3, size: undefined, _success: true },

    // Zero-sized bindings
    { offset: 0, size: 0, _success: false },
    { offset: 256, size: 0, _success: false },
    { offset: 1024, size: 0, _success: false },
    { offset: 1024, size: undefined, _success: false },

    // Unaligned buffer offset is invalid
    { offset: 1, size: 256, _success: false },
    { offset: 1, size: undefined, _success: false },
    { offset: 128, size: 256, _success: false },
    { offset: 255, size: 256, _success: false },

    // Out-of-bounds
    { offset: 256 * 5, size: 0, _success: false }, // offset is OOB
    { offset: 0, size: 256 * 5, _success: false }, // size is OOB
    { offset: 1024, size: 1, _success: false }, // offset+size is OOB
  ])
  .fn(async t => {
    const { offset, size, _success } = t.params;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [{ binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: 'storage' } }],
    });

    const buffer = t.device.createBuffer({
      size: 1024,
      usage: GPUBufferUsage.STORAGE,
    });

    const descriptor = {
      entries: [
        {
          binding: 0,
          resource: { buffer, offset, size },
        },
      ],

      layout: bindGroupLayout,
    };

    if (_success) {
      // Control case
      t.device.createBindGroup(descriptor);
    } else {
      // Buffer offset and/or size don't match in bind groups.
      t.expectValidationError(() => {
        t.device.createBindGroup(descriptor);
      });
    }
  });

g.test('minBindingSize')
  .desc('Tests that minBindingSize is correctly enforced.')
  .paramsSubcasesOnly(u =>
    u //
      .combine('minBindingSize', [undefined, 4, 256])
      .expand('size', ({ minBindingSize }) =>
        minBindingSize !== undefined
          ? [minBindingSize - 1, minBindingSize, minBindingSize + 1]
          : [4, 256]
      )
  )
  .fn(t => {
    const { size, minBindingSize } = t.params;

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUShaderStage.FRAGMENT,
          buffer: {
            type: 'storage',
            minBindingSize,
          },
        },
      ],
    });

    const storageBuffer = t.device.createBuffer({
      size,
      usage: GPUBufferUsage.STORAGE,
    });

    t.expectValidationError(() => {
      t.device.createBindGroup({
        layout: bindGroupLayout,
        entries: [
          {
            binding: 0,
            resource: {
              buffer: storageBuffer,
            },
          },
        ],
      });
    }, minBindingSize !== undefined && size < minBindingSize);
  });

g.test('buffer,resource_state')
  .desc('Test bind group creation with various buffer resource states')
  .paramsSubcasesOnly(u =>
    u.combine('state', kResourceStates).combine('entry', bufferBindingEntries(true))
  )
  .fn(t => {
    const { state, entry } = t.params;

    assert(entry.buffer !== undefined);
    const info = bufferBindingTypeInfo(entry.buffer);

    const bgl = t.device.createBindGroupLayout({
      entries: [
        {
          ...entry,
          binding: 0,
          visibility: info.validStages,
        },
      ],
    });

    const buffer = t.createBufferWithState(state, {
      usage: info.usage,
      size: 4,
    });

    t.expectValidationError(() => {
      t.device.createBindGroup({
        layout: bgl,
        entries: [
          {
            binding: 0,
            resource: {
              buffer,
            },
          },
        ],
      });
    }, state === 'invalid');
  });

g.test('texture,resource_state')
  .desc('Test bind group creation with various texture resource states')
  .paramsSubcasesOnly(u =>
    u
      .combine('state', kResourceStates)
      .combine('entry', sampledAndStorageBindingEntries(true, 'rgba8unorm'))
  )
  .fn(t => {
    const { state, entry } = t.params;
    const info = texBindingTypeInfo(entry);

    const bgl = t.device.createBindGroupLayout({
      entries: [
        {
          ...entry,
          binding: 0,
          visibility: info.validStages,
        },
      ],
    });

    const texture = t.createTextureWithState(state, {
      usage: info.usage,
      size: [1, 1],
      format: 'rgba8unorm',
      sampleCount: entry.texture?.multisampled ? 4 : 1,
    });

    let textureView;
    t.expectValidationError(() => {
      textureView = texture.createView();
    }, state === 'invalid');

    t.expectValidationError(() => {
      t.device.createBindGroup({
        layout: bgl,
        entries: [
          {
            binding: 0,
            resource: textureView,
          },
        ],
      });
    }, state === 'invalid');
  });

g.test('bind_group_layout,device_mismatch')
  .desc(
    'Tests createBindGroup cannot be called with a bind group layout created from another device'
  )
  .paramsSubcasesOnly(u => u.combine('mismatched', [true, false]))
  .fn(async t => {
    const mismatched = t.params.mismatched;

    if (mismatched) {
      await t.selectMismatchedDeviceOrSkipTestCase(undefined);
    }

    const device = mismatched ? t.mismatchedDevice : t.device;

    const bgl = device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: GPUConst.ShaderStage.VERTEX,
          buffer: {},
        },
      ],
    });

    t.expectValidationError(() => {
      t.device.createBindGroup({
        layout: bgl,
        entries: [
          {
            binding: 0,
            resource: { buffer: t.getUniformBuffer() },
          },
        ],
      });
    }, mismatched);
  });

g.test('binding_resources,device_mismatch')
  .desc(
    `
    Tests createBindGroup cannot be called with various resources created from another device
    Test with two resources to make sure all resources can be validated:
    - resource0 and resource1 from same device
    - resource0 and resource1 from different device

    TODO: test GPUExternalTexture as a resource
    `
  )
  .params(u =>
    u
      .combine('entry', [
        { buffer: { type: 'storage' } },
        { sampler: { type: 'filtering' } },
        { texture: { multisampled: false } },
        { storageTexture: { access: 'write-only', format: 'rgba8unorm' } },
      ])
      .beginSubcases()
      .combineWithParams([
        { resource0Mismatched: false, resource1Mismatched: false }, //control case
        { resource0Mismatched: true, resource1Mismatched: false },
        { resource0Mismatched: false, resource1Mismatched: true },
      ])
  )
  .fn(async t => {
    const { entry, resource0Mismatched, resource1Mismatched } = t.params;

    if (resource0Mismatched || resource1Mismatched) {
      await t.selectMismatchedDeviceOrSkipTestCase(undefined);
    }

    const info = bindingTypeInfo(entry);

    const resource0 = resource0Mismatched
      ? t.getDeviceMismatchedBindingResource(info.resource)
      : t.getBindingResource(info.resource);
    const resource1 = resource1Mismatched
      ? t.getDeviceMismatchedBindingResource(info.resource)
      : t.getBindingResource(info.resource);

    const bgl = t.device.createBindGroupLayout({
      entries: [
        {
          binding: 0,
          visibility: info.validStages,
          ...entry,
        },

        {
          binding: 1,
          visibility: info.validStages,
          ...entry,
        },
      ],
    });

    t.expectValidationError(() => {
      t.device.createBindGroup({
        layout: bgl,
        entries: [
          {
            binding: 0,
            resource: resource0,
          },

          {
            binding: 1,
            resource: resource1,
          },
        ],
      });
    }, resource0Mismatched || resource1Mismatched);
  });
