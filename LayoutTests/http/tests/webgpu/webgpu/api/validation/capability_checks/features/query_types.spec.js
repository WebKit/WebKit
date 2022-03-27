/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for capability checking for features enabling optional query types.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('createQuerySet')
  .desc(
    `
Tests that creating query set shouldn't be valid without the required feature enabled.
- createQuerySet
  - type {occlusion, timestamp}
  - x= {pipeline statistics, timestamp} query {enable, disable}

TODO: This test should expect *synchronous* exceptions, not validation errors, per
<https://github.com/gpuweb/gpuweb/blob/main/design/ErrorConventions.md>.
As of this writing, the spec needs to be fixed as well.
  `
  )
  .params(u =>
    u.combine('type', ['occlusion', 'timestamp']).combine('timestampQueryEnable', [false, true])
  )
  .fn(async t => {
    const { type, timestampQueryEnable } = t.params;

    const requiredFeatures = [];
    if (timestampQueryEnable) {
      requiredFeatures.push('timestamp-query');
    }

    await t.selectDeviceOrSkipTestCase({ requiredFeatures });

    const count = 1;
    const shouldError = type === 'timestamp' && !timestampQueryEnable;

    t.expectValidationError(() => {
      t.device.createQuerySet({ type, count });
    }, shouldError);
  });
