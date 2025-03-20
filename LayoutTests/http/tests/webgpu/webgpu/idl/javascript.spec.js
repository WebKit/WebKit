/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validate that various GPU objects behave correctly in JavaScript.

Examples:
  - GPUTexture, GPUBuffer, GPUQuerySet, GPUDevice, GPUAdapter, GPUAdapter.limits, GPUDevice.limits
    - return nothing for Object.keys()
    - adds no properties for {...object}
    - iterate over keys for (key in object)
    - throws for (key of object)
    - return nothing from Object.getOwnPropertyDescriptors
 - GPUAdapter.limits, GPUDevice.limits
     - can not be passed to requestAdapter as requiredLimits
 - GPUAdapter.features, GPUDevice.features
    - do spread to array
    - can be copied to set
    - can be passed to requestAdapter as requiredFeatures
`;import { makeTestGroup } from '../../common/framework/test_group.js';
import { keysOf } from '../../common/util/data_tables.js';
import { getGPU } from '../../common/util/navigator_gpu.js';
import { assert, objectEquals, unreachable } from '../../common/util/util.js';
import { getDefaultLimitsForAdapter, kLimits } from '../capability_info.js';
import { AllFeaturesMaxLimitsGPUTest } from '../gpu_test.js';

// MAINTENANCE_TODO: Remove this filter when these limits are added to the spec.
const isUnspecifiedLimit = (limit) =>
/maxStorage(Buffer|Texture)sIn(Vertex|Fragment)Stage/.test(limit);

const kSpecifiedLimits = kLimits.filter((s) => !isUnspecifiedLimit(s));








const kResourceInfo = {
  gpu: {
    create(t) {
      return getGPU(null);
    },
    requiredKeys: ['wgslLanguageFeatures', 'requestAdapter'],
    getters: ['wgslLanguageFeatures'],
    settable: [],
    sameObject: ['wgslLanguageFeatures']
  },
  buffer: {
    create(t) {
      return t.createBufferTracked({ size: 16, usage: GPUBufferUsage.UNIFORM });
    },
    requiredKeys: [
    'destroy',
    'getMappedRange',
    'label',
    'mapAsync',
    'mapState',
    'size',
    'unmap',
    'usage'],

    getters: ['label', 'mapState', 'size', 'usage'],
    settable: ['label'],
    sameObject: []
  },
  texture: {
    create(t) {
      return t.createTextureTracked({
        size: [2, 3],
        format: 'r8unorm',
        usage: GPUTextureUsage.TEXTURE_BINDING
      });
    },
    requiredKeys: [
    'createView',
    'depthOrArrayLayers',
    'destroy',
    'dimension',
    'format',
    'height',
    'label',
    'mipLevelCount',
    'sampleCount',
    'usage',
    'width'],

    getters: [
    'depthOrArrayLayers',
    'dimension',
    'format',
    'height',
    'label',
    'mipLevelCount',
    'sampleCount',
    'usage',
    'width'],

    settable: ['label'],
    sameObject: []
  },
  querySet: {
    create(t) {
      return t.createQuerySetTracked({
        type: 'occlusion',
        count: 2
      });
    },
    requiredKeys: ['count', 'destroy', 'label', 'type'],
    getters: ['count', 'label', 'type'],
    settable: ['label'],
    sameObject: []
  },
  adapter: {
    create(t) {
      return t.adapter;
    },
    requiredKeys: ['features', 'info', 'limits', 'requestDevice'],
    getters: ['features', 'info', 'limits'],
    settable: [],
    sameObject: ['features', 'info', 'limits']
  },
  device: {
    create(t) {
      return t.device;
    },
    requiredKeys: [
    'adapterInfo',
    'addEventListener',
    'createBindGroup',
    'createBindGroupLayout',
    'createBuffer',
    'createCommandEncoder',
    'createComputePipeline',
    'createComputePipelineAsync',
    'createPipelineLayout',
    'createQuerySet',
    'createRenderBundleEncoder',
    'createRenderPipeline',
    'createRenderPipelineAsync',
    'createSampler',
    'createShaderModule',
    'createTexture',
    'destroy',
    'dispatchEvent',
    'features',
    'importExternalTexture',
    'label',
    'limits',
    'lost',
    'onuncapturederror',
    'popErrorScope',
    'pushErrorScope',
    'queue',
    'removeEventListener'],

    getters: ['adapterInfo', 'features', 'label', 'limits', 'lost', 'onuncapturederror', 'queue'],
    settable: ['label', 'onuncapturederror'],
    sameObject: ['adapterInfo', 'features', 'limits', 'queue']
  },
  'adapter.limits': {
    create(t) {
      return t.adapter.limits;
    },
    requiredKeys: kSpecifiedLimits,
    getters: kSpecifiedLimits,
    settable: [],
    sameObject: []
  },
  'device.limits': {
    create(t) {
      return t.device.limits;
    },
    requiredKeys: kSpecifiedLimits,
    getters: kSpecifiedLimits,
    settable: [],
    sameObject: []
  }
};
const kResources = keysOf(kResourceInfo);


function createResource(t, type) {
  return kResourceInfo[type].create(t);
}



function forOfIterations(obj) {
  let count = 0;
  for (const _ of obj) {
    ++count;
  }
  return count;
}

function hasRequiredKeys(t, obj, requiredKeys) {
  for (const requiredKey of requiredKeys) {
    t.expect(requiredKey in obj, `${requiredKey} in ${obj.constructor.name} exists`);
  }
}

function aHasBElements(
t,
a,
b)
{
  for (const elem of b) {
    if (Array.isArray(a)) {
      t.expect(a.includes(elem), `missing ${elem}`);
    } else if (a.has) {
      t.expect(a.has(elem), `missing ${elem}`);
    } else {
      unreachable();
    }
  }
}

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);
g.test('obj,Object_keys').
desc('tests returns nothing for Object.keys()').
params((u) => u.combine('type', kResources)).
fn((t) => {
  const { type } = t.params;
  const obj = createResource(t, type);
  t.expect(objectEquals([...Object.keys(obj)], []), `[...Object.keys(${type})] === []`);
});

g.test('obj,spread').
desc('does not spread').
params((u) => u.combine('type', kResources)).
fn((t) => {
  const { type } = t.params;
  const obj = createResource(t, type);
  t.expect(objectEquals({ ...obj }, {}), `{ ...${type} ] === {}`);
});

g.test('obj,for_in').
desc('iterates over keys - for (key in object)').
params((u) => u.combine('type', kResources)).
fn((t) => {
  const { type } = t.params;
  const obj = createResource(t, type);
  hasRequiredKeys(t, obj, kResourceInfo[type].requiredKeys);
});

g.test('obj,for_of').
desc('throws TypeError - for (key of object').
params((u) => u.combine('type', kResources)).
fn((t) => {
  const { type } = t.params;
  const obj = createResource(t, type);
  t.shouldThrow('TypeError', () => forOfIterations(obj), {
    message: `for (const key of ${type} } throws TypeError`
  });
});

g.test('obj,getOwnPropertyDescriptors').
desc('Object.getOwnPropertyDescriptors returns {}').
params((u) => u.combine('type', kResources)).
fn((t) => {
  const { type } = t.params;
  const obj = createResource(t, type);
  t.expect(
    objectEquals(Object.getOwnPropertyDescriptors(obj), {}),
    `Object.getOwnPropertyDescriptors(${type}} === {}`
  );
});

const kSetLikeFeaturesInfo = {
  adapterFeatures(t) {
    return t.adapter.features;
  },
  deviceFeatures(t) {
    return t.device.features;
  }
};
const kSetLikeFeatures = keysOf(kSetLikeFeaturesInfo);

const kSetLikeInfo = {
  ...kSetLikeFeaturesInfo,
  wgslLanguageFeatures() {
    return getGPU(null).wgslLanguageFeatures;
  }
};
const kSetLikes = keysOf(kSetLikeInfo);

g.test('setlike,spread').
desc('obj spreads').
params((u) => u.combine('type', kSetLikes)).
fn((t) => {
  const { type } = t.params;
  const setLike = kSetLikeInfo[type](t);
  const copy = [...setLike];
  aHasBElements(t, copy, setLike);
  aHasBElements(t, setLike, copy);
});

g.test('setlike,set').
desc('obj copies to set').
params((u) => u.combine('type', kSetLikes)).
fn((t) => {
  const { type } = t.params;
  const setLike = kSetLikeInfo[type](t);
  const copy = new Set(setLike);
  aHasBElements(t, copy, setLike);
  aHasBElements(t, setLike, copy);
});

g.test('setlike,requiredFeatures').
desc('can be passed as required features').
params((u) => u.combine('type', kSetLikeFeatures)).
fn(async (t) => {
  const { type } = t.params;
  const features = kSetLikeFeaturesInfo[type](t);

  const gpu = getGPU(null);
  const adapter = await gpu.requestAdapter();
  const device = await t.requestDeviceTracked(adapter, {
    requiredFeatures: features
  });
  aHasBElements(t, device.features, features);
  aHasBElements(t, features, device.features);
});

g.test('limits').
desc('adapter/device.limits can not be passed as requiredLimits').
params((u) => u.combine('type', ['adapter', 'device'])).
fn(async (t) => {
  const { type } = t.params;
  const obj = type === 'adapter' ? t.adapter : t.device;

  const gpu = getGPU(null);
  const adapter = await gpu.requestAdapter();
  assert(!!adapter);
  const device = await t.requestDeviceTracked(adapter, {
    requiredLimits: obj.limits
  });
  const defaultLimits = getDefaultLimitsForAdapter(adapter);
  for (const [key, { default: defaultLimit }] of Object.entries(defaultLimits)) {
    if (isUnspecifiedLimit(key)) {
      continue;
    }
    const actual = device.limits[key];
    t.expect(
      actual === defaultLimit,
      `expected device.limits.${key}(${actual}) === ${defaultLimit}`
    );
  }
});

g.test('readonly_properties').
desc(
  `
    Test that setting a property with no setter throws.
    `
).
params((u) => u.combine('type', kResources)).
fn((t) => {
  'use strict'; // This makes setting a readonly property produce a TypeError.
  const { type } = t.params;
  const { getters, settable } = kResourceInfo[type];
  const obj = createResource(t, type);
  for (const getter of getters) {
    const origValue = obj[getter];

    // try setting it.
    const isSettable = settable.includes(getter);
    t.shouldThrow(
      isSettable ? false : 'TypeError',
      () => {
        obj[getter] = 'test value';
        // If we were able to set it, restore it.
        obj[getter] = origValue;
      },
      {
        message: `instanceof ${type}.${getter} = value ${
        isSettable ? 'does not throw' : 'throws'
        }`
      }
    );
  }
});

g.test('getter_replacement').
desc(
  `
    Test that replacing getters on class prototypes works

    This is a common pattern for shims and debugging libraries so make sure this pattern works.
    `
).
params((u) => u.combine('type', kResources)).
fn((t) => {
  const { type } = t.params;
  const { getters } = kResourceInfo[type];

  const obj = createResource(t, type);
  for (const getter of getters) {
    // Check it's not 'ownProperty`
    const properties = Object.getOwnPropertyDescriptor(obj, getter);
    t.expect(
      properties === undefined,
      `Object.getOwnPropertyDescriptor(instance of ${type}, '${getter}') === undefined`
    );

    // Check it's actually a getter that returns a non-function value.
    const origValue = obj[getter];
    t.expect(typeof origValue !== 'function', `instance of ${type}.${getter} !== 'function'`);

    // check replacing the getter on constructor works.
    const ctorPrototype = obj.constructor.prototype;
    const origProperties = Object.getOwnPropertyDescriptor(ctorPrototype, getter);
    assert(
      !!origProperties,
      `Object.getOwnPropertyDescriptor(${type}, '${getter}') !== undefined`
    );
    try {
      Object.defineProperty(ctorPrototype, getter, {
        get() {
          return 'testGetterValue';
        }
      });
      const value = obj[getter];
      t.expect(
        value === 'testGetterValue',
        `replacing getter: '${getter}' on ${type} returns test value`
      );
    } finally {
      Object.defineProperty(ctorPrototype, getter, origProperties);
    }

    // Check it turns the same value after restoring as before restoring.
    const afterValue = obj[getter];
    assert(afterValue === origValue, `able to restore getter for instance of ${type}.${getter}`);

    // Check getOwnProperty also returns the value we got before.
    assert(
      Object.getOwnPropertyDescriptor(ctorPrototype, getter).get === origProperties.get,
      `getOwnPropertyDescriptor(${type}, '${getter}').get is original function`
    );
  }
});

g.test('method_replacement').
desc(
  `
    Test that replacing methods on class prototypes works

    This is a common pattern for shims and debugging libraries so make sure this pattern works.
    `
).
params((u) => u.combine('type', kResources)).
fn((t) => {
  const { type } = t.params;
  const { requiredKeys, getters } = kResourceInfo[type];
  const gettersSet = new Set(getters);
  const methods = requiredKeys.filter((k) => !gettersSet.has(k));

  const obj = createResource(t, type);
  for (const method of methods) {
    const ctorPrototype = obj.constructor.prototype;
    const origFunc = ctorPrototype[method];

    t.expect(typeof origFunc === 'function', `${type}.prototype.${method} is a function`);

    // Check the function the prototype and the one on the object are the same
    t.expect(
      obj[method] === origFunc,
      `instance of ${type}.${method} === ${type}.prototype.${method}`
    );

    // Check replacing the method on constructor works.
    try {
      ctorPrototype[method] = function () {
        return 'testMethodValue';
      };
      const value = obj[method]();
      t.expect(
        value === 'testMethodValue',
        `replacing method: '${method}' on ${type} returns test value`
      );
    } finally {
      ctorPrototype[method] = origFunc;
    }

    // Check the function the prototype and the one on the object are the same after restoring.
    assert(
      obj[method] === origFunc,
      `instance of ${type}.${method} === ${type}.prototype.${method}`
    );
  }
});

g.test('sameObject').
desc(
  `
    Test that property that are supposed to return the sameObject do.
    `
).
params((u) => u.combine('type', kResources)).
fn((t) => {
  const { type } = t.params;
  const { sameObject } = kResourceInfo[type];
  const obj = createResource(t, type);
  for (const property of sameObject) {
    const origValue1 = obj[property];
    const origValue2 = obj[property];
    t.expect(typeof origValue1 === 'object');
    t.expect(typeof origValue2 === 'object');
    // Seems like this should be enough if they are objects.
    t.expect(origValue1 === origValue2);
    // add a property
    origValue1['foo'] = 'test';
    // see that it appears on the object.
    t.expect(origValue1.foo === 'test');
    t.expect(origValue2.foo === 'test');
    // Delete the property.
    delete origValue2.foo;
    // See it was removed.
    assert(origValue1.foo === undefined);
    assert(origValue2.foo === undefined);
  }
});