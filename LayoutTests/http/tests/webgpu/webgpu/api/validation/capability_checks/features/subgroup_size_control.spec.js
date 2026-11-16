/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Tests for capability checking for the 'subgroup-size-control' feature.

Test that enabling the 'subgroup-size-control' feature also enables the 'subgroups' feature.
`;import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { hasFeature } from '../../../../../common/util/util.js';
import { UniqueFeaturesOrLimitsGPUTest } from '../../../../gpu_test.js';

export const g = makeTestGroup(UniqueFeaturesOrLimitsGPUTest);

g.test('enables_subgroups').
desc(
  `
  Test that enabling the 'subgroup-size-control' feature also enables the 'subgroups' feature.
  `
).
beforeAllSubcases((t) => t.selectDeviceOrSkipTestCase('subgroup-size-control')).
fn((t) => {
  t.expect(() => hasFeature(t.device.features, 'subgroups'));
});