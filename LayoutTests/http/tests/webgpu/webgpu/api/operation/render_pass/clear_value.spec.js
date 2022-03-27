/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for render pass clear values.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('stored')
  .desc(`Test render pass clear values are stored at the end of an empty pass.`)
  .unimplemented();

g.test('loaded')
  .desc(
    `Test render pass clear values are visible during the pass by doing some trivial blending
with the attachment (e.g. add [0,0,0,0] to the color and verify the stored result).`
  )
  .unimplemented();

g.test('srgb')
  .desc(
    `Test that clear values on '-srgb' type attachments are interpreted as unencoded (linear),
not decoded from srgb to linear.`
  )
  .unimplemented();

g.test('layout')
  .desc(
    `Test that bind group layouts of the default pipeline layout are correct by passing various
shaders and then checking their computed bind group layouts are compatible with particular bind
groups.`
  )
  .unimplemented();
