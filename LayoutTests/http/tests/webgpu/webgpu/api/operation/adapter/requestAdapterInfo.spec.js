/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests various ways of calling GPUAdapter.requestAdapterInfo.

TODO:
- Find a way to perform tests with and without user activation
`;
import { Fixture } from '../../../../common/framework/fixture.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { getGPU } from '../../../../common/util/navigator_gpu.js';
import { assert } from '../../../../common/util/util.js';

export const g = makeTestGroup(Fixture);

const normalizedIdentifierRegex = /^$|^[a-z0-9]+(-[a-z0-9]+)*$/;

g.test('adapter_info')
  .desc(
    `
  Test that calling requestAdapterInfo with no arguments:
    - Returns a GPUAdapterInfo structure
    - Every member in the structure except description is properly formatted`
  )
  .fn(async t => {
    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    const adapterInfo = await adapter.requestAdapterInfo();

    t.expect(
      normalizedIdentifierRegex.test(adapterInfo.vendor),
      'adapterInfo.vendor should be a normalized identifier'
    );

    t.expect(
      normalizedIdentifierRegex.test(adapterInfo.architecture),
      'adapterInfo.architecture should be a normalized identifier'
    );

    t.expect(
      normalizedIdentifierRegex.test(adapterInfo.device),
      'adapterInfo.device should be a normalized identifier'
    );
  });

g.test('adapter_info_with_hints')
  .desc(
    `
  Test that calling requestAdapterInfo with hints:
    - Rejects without user activation
    - Succeed with user activation`
  )
  .unimplemented();
