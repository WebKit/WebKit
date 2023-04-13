/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for validation in beginComputePass and GPUComputePassDescriptor as its optional descriptor.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { kQueryTypes } from '../../../capability_info.js';
import { ValidationTest } from '../validation_test.js';

class F extends ValidationTest {
  tryComputePass(success, descriptor) {
    const encoder = this.device.createCommandEncoder();
    const computePass = encoder.beginComputePass(descriptor);
    computePass.end();

    this.expectValidationError(() => {
      encoder.finish();
    }, !success);
  }
}

export const g = makeTestGroup(F);

g.test('timestampWrites,same_location')
  .desc(
    `
  Test that entries in timestampWrites do not have the same location in GPUComputePassDescriptor.
  `
  )
  .params(u =>
    u //
      .combine('locationA', ['beginning', 'end'])
      .combine('locationB', ['beginning', 'end'])
  )
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase(['timestamp-query']);
  })
  .fn(t => {
    const { locationA, locationB } = t.params;

    const querySet = t.device.createQuerySet({
      type: 'timestamp',
      count: 2,
    });

    const timestampWriteA = {
      querySet,
      queryIndex: 0,
      location: locationA,
    };

    const timestampWriteB = {
      querySet,
      queryIndex: 1,
      location: locationB,
    };

    const isValid = locationA !== locationB;

    const descriptor = {
      timestampWrites: [timestampWriteA, timestampWriteB],
    };

    t.tryComputePass(isValid, descriptor);
  });

g.test('timestampWrites,query_set_type')
  .desc(
    `
  Test that all entries of the timestampWrites must have type 'timestamp'. If all query types are
  not 'timestamp' in GPUComputePassDescriptor, a validation error should be generated.
  `
  )
  .params(u =>
    u //
      .combine('queryTypeA', kQueryTypes)
      .combine('queryTypeB', kQueryTypes)
  )
  .beforeAllSubcases(t => {
    t.selectDeviceForQueryTypeOrSkipTestCase([
      'timestamp',
      t.params.queryTypeA,
      t.params.queryTypeB,
    ]);
  })
  .fn(t => {
    const { queryTypeA, queryTypeB } = t.params;

    const timestampWriteA = {
      querySet: t.device.createQuerySet({ type: queryTypeA, count: 1 }),
      queryIndex: 0,
      location: 'beginning',
    };

    const timestampWriteB = {
      querySet: t.device.createQuerySet({ type: queryTypeB, count: 1 }),
      queryIndex: 0,
      location: 'end',
    };

    const isValid = queryTypeA === 'timestamp' && queryTypeB === 'timestamp';

    const descriptor = {
      timestampWrites: [timestampWriteA, timestampWriteB],
    };

    t.tryComputePass(isValid, descriptor);
  });

g.test('timestampWrites,invalid_query_set')
  .desc(`Tests that timestampWrite that has an invalid query set generates a validation error.`)
  .params(u => u.combine('querySetState', ['valid', 'invalid']))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase(['timestamp-query']);
  })
  .fn(t => {
    const { querySetState } = t.params;

    const querySet = t.createQuerySetWithState(querySetState, {
      type: 'timestamp',
      count: 1,
    });

    const timestampWrite = {
      querySet,
      queryIndex: 0,
      location: 'beginning',
    };

    const descriptor = {
      timestampWrites: [timestampWrite],
    };

    t.tryComputePass(querySetState === 'valid', descriptor);
  });

g.test('timestampWrites,query_index_count')
  .desc(`Test that querySet.count should be greater than timestampWrite.queryIndex.`)
  .params(u => u.combine('queryIndex', [0, 1, 2, 3]))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase(['timestamp-query']);
  })
  .fn(t => {
    const { queryIndex } = t.params;

    const querySetCount = 2;

    const timestampWrite = {
      querySet: t.device.createQuerySet({ type: 'timestamp', count: querySetCount }),
      queryIndex,
      location: 'beginning',
    };

    const isValid = queryIndex < querySetCount;

    const descriptor = {
      timestampWrites: [timestampWrite],
    };

    t.tryComputePass(isValid, descriptor);
  });

g.test('timestamp_query_set,device_mismatch')
  .desc(
    `
  Tests beginComputePass cannot be called with a timestamp query set created from another device.
  `
  )
  .paramsSubcasesOnly(u => u.combine('mismatched', [true, false]))
  .beforeAllSubcases(t => {
    t.selectDeviceOrSkipTestCase(['timestamp-query']);
    t.selectMismatchedDeviceOrSkipTestCase('timestamp-query');
  })
  .fn(t => {
    const { mismatched } = t.params;
    const sourceDevice = mismatched ? t.mismatchedDevice : t.device;

    const timestampQuerySet = sourceDevice.createQuerySet({
      type: 'timestamp',
      count: 1,
    });

    const timestampWrite = {
      querySet: timestampQuerySet,
      queryIndex: 0,
      location: 'beginning',
    };

    const descriptor = {
      timestampWrites: [timestampWrite],
    };

    t.tryComputePass(!mismatched, descriptor);
  });
