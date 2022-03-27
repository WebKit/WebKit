/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for GPUAdapter.requestDevice.

TODO:
- descriptor is {null, undefined}
- descriptor with the combinations of features
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('request_default_after_error')
  .desc(`Request default device after a failed request with unsupported features.`)
  .unimplemented();
