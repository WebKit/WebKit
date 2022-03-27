/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for validation in createQuerySet.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { kQueryTypes, kMaxQueryCount } from '../../../capability_info.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('count')
  .desc(
    `
Tests that create query set with the count for all query types:
- count {<, =, >} kMaxQueryCount
- x= {occlusion, timestamp} query
  `
  )
  .params(u =>
    u
      .combine('type', kQueryTypes)
      .beginSubcases()
      .combine('count', [0, kMaxQueryCount, kMaxQueryCount + 1])
  )
  .fn(async t => {
    const { type, count } = t.params;

    await t.selectDeviceForQueryTypeOrSkipTestCase(type);

    t.expectValidationError(() => {
      t.device.createQuerySet({ type, count });
    }, count > kMaxQueryCount);
  });
