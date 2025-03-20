/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation for encoding queries.
`;import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { kQueryTypes } from '../../../../capability_info.js';
import { AllFeaturesMaxLimitsValidationTest } from '../../validation_test.js';

import { createQuerySetWithType } from './common.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsValidationTest);

g.test('occlusion_query,query_type').
desc(
  `
Tests that set occlusion query set with all types in render pass descriptor:
- type {occlusion (control case), timestamp}
- {undefined} for occlusion query set in render pass descriptor
  `
).
params((u) => u.combine('type', [undefined, ...kQueryTypes])).
fn((t) => {
  const type = t.params.type;
  if (type) {
    t.skipIfDeviceDoesNotSupportQueryType(type);
  }
  const querySet = type === undefined ? undefined : createQuerySetWithType(t, type, 1);

  const encoder = t.createEncoder('render pass', { occlusionQuerySet: querySet });
  encoder.encoder.beginOcclusionQuery(0);
  encoder.encoder.endOcclusionQuery();
  encoder.validateFinish(type === 'occlusion');
});

g.test('occlusion_query,invalid_query_set').
desc(
  `
Tests that begin occlusion query with a invalid query set that failed during creation.
  `
).
paramsSubcasesOnly((u) => u.combine('querySetState', ['valid', 'invalid'])).
fn((t) => {
  const occlusionQuerySet = t.createQuerySetWithState(t.params.querySetState);

  const encoder = t.createEncoder('render pass', { occlusionQuerySet });
  encoder.encoder.beginOcclusionQuery(0);
  encoder.encoder.endOcclusionQuery();
  encoder.validateFinishAndSubmitGivenState(t.params.querySetState);
});

g.test('occlusion_query,query_index').
desc(
  `
Tests that begin occlusion query with query index:
- queryIndex {in, out of} range for GPUQuerySet
  `
).
paramsSubcasesOnly((u) => u.combine('queryIndex', [0, 2])).
fn((t) => {
  const occlusionQuerySet = createQuerySetWithType(t, 'occlusion', 2);

  const encoder = t.createEncoder('render pass', { occlusionQuerySet });
  encoder.encoder.beginOcclusionQuery(t.params.queryIndex);
  encoder.encoder.endOcclusionQuery();
  encoder.validateFinish(t.params.queryIndex < 2);
});

g.test('writeTimestamp,query_type_and_index').
desc(
  `
Tests that write timestamp to all types of query set on all possible encoders:
- type {occlusion, timestamp}
- queryIndex {in, out of} range for GPUQuerySet
- x= {non-pass} encoder

TODO: writeTimestamp is removed from the spec so it's skipped if it TypeErrors.
`
).
params((u) =>
u.
combine('type', kQueryTypes).
beginSubcases().
expand('queryIndex', (p) => p.type === 'timestamp' ? [0, 2] : [0])
).
fn((t) => {
  const { type, queryIndex } = t.params;
  t.skipIfDeviceDoesNotSupportQueryType('timestamp');
  t.skipIfDeviceDoesNotSupportQueryType(type);

  const count = 2;
  const querySet = createQuerySetWithType(t, type, count);

  const encoder = t.createEncoder('non-pass');
  try {

    encoder.encoder.writeTimestamp(querySet, queryIndex);
  } catch (ex) {
    t.skipIf(ex instanceof TypeError, 'writeTimestamp is actually not available');
  }
  encoder.validateFinish(type === 'timestamp' && queryIndex < count);
});

g.test('writeTimestamp,invalid_query_set').
desc(
  `
Tests that write timestamp to a invalid query set that failed during creation:
- x= {non-pass} encoder

TODO: writeTimestamp is removed from the spec so it's skipped if it TypeErrors.
`
).
paramsSubcasesOnly((u) => u.combine('querySetState', ['valid', 'invalid'])).
fn((t) => {
  t.skipIfDeviceDoesNotSupportQueryType('timestamp');
  const { querySetState } = t.params;

  const querySet = t.createQuerySetWithState(querySetState, {
    type: 'timestamp',
    count: 2
  });

  const encoder = t.createEncoder('non-pass');
  try {

    encoder.encoder.writeTimestamp(querySet, 0);
  } catch (ex) {
    t.skipIf(ex instanceof TypeError, 'writeTimestamp is actually not available');
  }
  encoder.validateFinish(querySetState !== 'invalid');
});

g.test('writeTimestamp,device_mismatch').
desc(
  `Tests writeTimestamp cannot be called with a query set created from another device

  TODO: writeTimestamp is removed from the spec so it's skipped if it TypeErrors.
  `
).
paramsSubcasesOnly((u) => u.combine('mismatched', [true, false])).
beforeAllSubcases((t) => t.usesMismatchedDevice()).
fn((t) => {
  t.skipIfDeviceDoesNotSupportQueryType('timestamp');
  const { mismatched } = t.params;
  const sourceDevice = mismatched ? t.mismatchedDevice : t.device;

  const querySet = t.trackForCleanup(
    sourceDevice.createQuerySet({
      type: 'timestamp',
      count: 2
    })
  );

  const encoder = t.createEncoder('non-pass');
  try {

    encoder.encoder.writeTimestamp(querySet, 0);
  } catch (ex) {
    t.skipIf(ex instanceof TypeError, 'writeTimestamp is actually not available');
  }
  encoder.validateFinish(!mismatched);
});