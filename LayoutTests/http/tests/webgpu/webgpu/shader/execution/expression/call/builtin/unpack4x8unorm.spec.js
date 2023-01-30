/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Decomposes a 32-bit value into four 8-bit chunks, then reinterprets each chunk
as an unsigned normalized floating point value.
Component i of the result is v ÷ 255, where v is the interpretation of bits 8×i
through 8×i+7 of e as an unsigned integer.
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { TypeF32, TypeU32, TypeVec } from '../../../../../util/conversion.js';
import { unpack4x8unormInterval } from '../../../../../util/f32_interval.js';
import { fullU32Range } from '../../../../../util/math.js';
import { makeCaseCache } from '../../case_cache.js';
import { allInputSources, generateU32ToVectorCases, run } from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

export const d = makeCaseCache('unpack4x8unorm', {
  u32_const: () => {
    return generateU32ToVectorCases(fullU32Range(), 'f32-only', unpack4x8unormInterval);
  },
  u32_non_const: () => {
    return generateU32ToVectorCases(fullU32Range(), 'unfiltered', unpack4x8unormInterval);
  },
});

g.test('unpack')
  .specURL('https://www.w3.org/TR/WGSL/#unpack-builtin-functions')
  .desc(
    `
@const fn unpack4x8unorm(e: u32) -> vec4<f32>
`
  )
  .params(u => u.combine('inputSource', allInputSources))
  .fn(async t => {
    const cases = await d.get(t.params.inputSource === 'const' ? 'u32_const' : 'u32_non_const');
    await run(t, builtin('unpack4x8unorm'), [TypeU32], TypeVec(4, TypeF32), t.params, cases);
  });
