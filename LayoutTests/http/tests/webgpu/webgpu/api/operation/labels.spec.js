/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for object labels.
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { GPUTest } from '../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('object_has_descriptor_label')
  .desc(`For every create function, the descriptor.label is carried over to the object.label.`)
  .unimplemented();
