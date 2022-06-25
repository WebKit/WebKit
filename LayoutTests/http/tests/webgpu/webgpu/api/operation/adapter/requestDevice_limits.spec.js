/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests passing various requiredLimits to GPUAdapter.requestDevice.
`;
import { Fixture } from '../../../../common/framework/fixture.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { getGPU } from '../../../../common/util/navigator_gpu.js';
import { assert } from '../../../../common/util/util.js';
import { kLimitInfo, kLimits } from '../../../capability_info.js';
import { clamp, isPowerOfTwo } from '../../../util/math.js';

export const g = makeTestGroup(Fixture);

g.test('unknown_limits')
  .desc(
    `
    Test that specifying limits that aren't part of the supported limit set causes
    requestDevice to reject.`
  )
  .fn(async t => {
    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    const requiredLimits = { unknownLimitName: 9000 };

    t.shouldReject('OperationError', adapter.requestDevice({ requiredLimits }));
  });

g.test('supported_limits')
  .desc(
    `
    Test that each supported limit can be specified with valid values.
    - Tests each limit with the default values given by the spec
    - Tests each limit with the supported values given by the adapter`
  )
  .params(u =>
    u.combine('limit', kLimits).beginSubcases().combine('limitValue', ['default', 'adapter'])
  )
  .fn(async t => {
    const { limit, limitValue } = t.params;

    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    let value = -1;
    switch (limitValue) {
      case 'default':
        value = kLimitInfo[limit].default;
        break;
      case 'adapter':
        value = adapter.limits[limit];
        break;
    }

    const device = await adapter.requestDevice({ requiredLimits: { [limit]: value } });
    assert(device !== null);
    t.expect(
      device.limits[limit] === value,
      'Devices reported limit should match the required limit'
    );
  });

g.test('better_than_supported')
  .desc(
    `
    Test that specifying a better limit than what the adapter supports causes requestDevice to
    reject.
    - Tests each limit
    - Tests requesting better limits by various amounts`
  )
  .params(u =>
    u
      .combine('limit', kLimits)
      .beginSubcases()
      .expandWithParams(p => {
        switch (kLimitInfo[p.limit].class) {
          case 'maximum':
            return [
              { mul: 1, add: 1 },
              { mul: 1, add: 100 },
            ];

          case 'alignment':
            return [
              { mul: 1, add: -1 },
              { mul: 1 / 2, add: 0 },
              { mul: 1 / 1024, add: 0 },
            ];
        }
      })
  )
  .fn(async t => {
    const { limit, mul, add } = t.params;

    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    const value = adapter.limits[limit] * mul + add;
    const requiredLimits = {
      [limit]: clamp(value, { min: 0, max: kLimitInfo[limit].maximumValue }),
    };

    t.shouldReject('OperationError', adapter.requestDevice({ requiredLimits }));
  });

g.test('worse_than_default')
  .desc(
    `
    Test that specifying a worse limit than the default values required by the spec cause the value
    to clamp.
    - Tests each limit
    - Tests requesting worse limits by various amounts`
  )
  .params(u =>
    u
      .combine('limit', kLimits)
      .beginSubcases()
      .expandWithParams(p => {
        switch (kLimitInfo[p.limit].class) {
          case 'maximum':
            return [
              { mul: 1, add: -1 },
              { mul: 1, add: -100 },
            ];

          case 'alignment':
            return [
              { mul: 1, add: 1 },
              { mul: 2, add: 0 },
              { mul: 1024, add: 0 },
            ];
        }
      })
  )
  .fn(async t => {
    const { limit, mul, add } = t.params;

    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    const value = kLimitInfo[limit].default * mul + add;
    const requiredLimits = {
      [limit]: clamp(value, { min: 0, max: kLimitInfo[limit].maximumValue }),
    };

    let success;
    switch (kLimitInfo[limit].class) {
      case 'alignment':
        success = isPowerOfTwo(value);
        break;
      case 'maximum':
        success = true;
        break;
    }

    if (success) {
      const device = await adapter.requestDevice({ requiredLimits });
      assert(device !== null);
      t.expect(
        device.limits[limit] === kLimitInfo[limit].default,
        'Devices reported limit should match the default limit'
      );
    } else {
      t.shouldReject('OperationError', adapter.requestDevice({ requiredLimits }));
    }
  });
