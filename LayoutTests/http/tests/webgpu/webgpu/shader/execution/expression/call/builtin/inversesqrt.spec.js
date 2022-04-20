/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the 'inverseSqrt' builtin function
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { ulpThreshold } from '../../../../../util/compare.js';
import { kBit, kValue } from '../../../../../util/constants.js';
import { f32, f32Bits, TypeF32 } from '../../../../../util/conversion.js';
import { biasedRange, linearRange } from '../../../../../util/math.js';
import { run } from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

g.test('f32')
  .uniqueId('84fc180ad82c5618')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
inverseSqrt:
T is f32 or vecN<f32> inverseSqrt(e: T ) -> T Returns the reciprocal of sqrt(e). Component-wise when T is a vector. (GLSLstd450InverseSqrt)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u =>
    u
      .combine('storageClass', ['uniform', 'storage_r', 'storage_rw'])
      .combine('vectorize', [undefined, 2, 3, 4])
  )
  .fn(async t => {
    // [1]: Need to decide what the ground-truth is.
    const makeCase = x => {
      return { input: f32(x), expected: f32(1 / Math.sqrt(x)) };
    };

    // Well defined cases
    const cases = [
      { input: f32Bits(kBit.f32.infinity.positive), expected: f32(0) },
      { input: f32(1), expected: f32(1) },
      // 0 < x <= 1 linearly spread
      ...linearRange(kValue.f32.positive.min, 1, 100).map(x => makeCase(x)),
      // 1 <= x < 2^32, biased towards 1
      ...biasedRange(1, 2 ** 32, 1000).map(x => makeCase(x)),
    ];

    const cfg = t.params;
    cfg.cmpFloats = ulpThreshold(2);
    run(t, builtin('inverseSqrt'), [TypeF32], TypeF32, cfg, cases);
  });
