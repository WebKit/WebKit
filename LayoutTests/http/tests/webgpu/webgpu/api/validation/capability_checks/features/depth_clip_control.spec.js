/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests checks that depend on 'depth-clip-control' being enabled.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('createRenderPipeline')
  .desc(`unclippedDepth=true requires 'depth-clip-control'; false or undefined does not.`)
  .unimplemented();
