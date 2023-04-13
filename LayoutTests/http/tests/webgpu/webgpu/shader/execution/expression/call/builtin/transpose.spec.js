/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution tests for the 'transpose' builtin function

T is AbstractFloat, f32, or f16
@const transpose(e: matRxC<T> ) -> matCxR<T>
Returns the transpose of e.
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { allInputSources } from '../../expression.js';

export const g = makeTestGroup(GPUTest);

g.test('abstract_float')
  .specURL('https://www.w3.org/TR/WGSL/#matrix-builtin-functions')
  .desc(`abstract float tests`)
  .params(u =>
    u.combine('inputSource', allInputSources).combine('rows', [2, 3, 4]).combine('cols', [2, 3, 4])
  )
  .unimplemented();

g.test('f32')
  .specURL('https://www.w3.org/TR/WGSL/#matrix-builtin-functions')
  .desc(`f32 tests`)
  .params(u =>
    u.combine('inputSource', allInputSources).combine('rows', [2, 3, 4]).combine('cols', [2, 3, 4])
  )
  .unimplemented();

g.test('f16')
  .specURL('https://www.w3.org/TR/WGSL/#matrix-builtin-functions')
  .desc(`f16 tests`)
  .params(u =>
    u.combine('inputSource', allInputSources).combine('rows', [2, 3, 4]).combine('cols', [2, 3, 4])
  )
  .unimplemented();
