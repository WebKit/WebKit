/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the 'ceil' builtin function
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { correctlyRoundedThreshold, anyOf } from '../../../../../util/compare.js';
import { kBit, kValue } from '../../../../../util/constants.js';
import { f32, f32Bits, TypeF32 } from '../../../../../util/conversion.js';
import { run } from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

g.test('f32')
  .uniqueId('38d65728ea728bc5')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
ceil:
T is f32 or vecN<f32> ceil(e: T ) -> T Returns the ceiling of e. Component-wise when T is a vector. (GLSLstd450Ceil)

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
    const cfg = t.params;
    cfg.cmpFloats = correctlyRoundedThreshold();

    run(t, builtin('ceil'), [TypeF32], TypeF32, cfg, [
      // Small positive numbers
      { input: f32(0.1), expected: f32(1.0) },
      { input: f32(0.9), expected: f32(1.0) },
      { input: f32(1.1), expected: f32(2.0) },
      { input: f32(1.9), expected: f32(2.0) },

      // Small negative numbers
      { input: f32(-0.1), expected: f32(0.0) },
      { input: f32(-0.9), expected: f32(0.0) },
      { input: f32(-1.1), expected: f32(-1.0) },
      { input: f32(-1.9), expected: f32(-1.0) },

      // Min and Max f32
      { input: f32Bits(kBit.f32.negative.max), expected: f32(0.0) },
      { input: f32Bits(kBit.f32.negative.min), expected: f32Bits(kBit.f32.negative.min) },
      { input: f32Bits(kBit.f32.positive.min), expected: f32(1.0) },
      { input: f32Bits(kBit.f32.positive.max), expected: f32Bits(kBit.f32.positive.max) },

      // Subnormal f32
      { input: f32Bits(kBit.f32.subnormal.positive.max), expected: anyOf(f32(1.0), f32(0.0)) },
      { input: f32Bits(kBit.f32.subnormal.positive.min), expected: anyOf(f32(1.0), f32(0.0)) },

      // Infinity f32
      { input: f32Bits(kBit.f32.infinity.negative), expected: f32Bits(kBit.f32.infinity.negative) },
      { input: f32Bits(kBit.f32.infinity.positive), expected: f32Bits(kBit.f32.infinity.positive) },

      // Powers of +2.0: 2.0^i: 1 <= i <= 31
      { input: f32(kValue.powTwo.to1), expected: f32(kValue.powTwo.to1) },
      { input: f32(kValue.powTwo.to2), expected: f32(kValue.powTwo.to2) },
      { input: f32(kValue.powTwo.to3), expected: f32(kValue.powTwo.to3) },
      { input: f32(kValue.powTwo.to4), expected: f32(kValue.powTwo.to4) },
      { input: f32(kValue.powTwo.to5), expected: f32(kValue.powTwo.to5) },
      { input: f32(kValue.powTwo.to6), expected: f32(kValue.powTwo.to6) },
      { input: f32(kValue.powTwo.to7), expected: f32(kValue.powTwo.to7) },
      { input: f32(kValue.powTwo.to8), expected: f32(kValue.powTwo.to8) },
      { input: f32(kValue.powTwo.to9), expected: f32(kValue.powTwo.to9) },
      { input: f32(kValue.powTwo.to10), expected: f32(kValue.powTwo.to10) },
      { input: f32(kValue.powTwo.to11), expected: f32(kValue.powTwo.to11) },
      { input: f32(kValue.powTwo.to12), expected: f32(kValue.powTwo.to12) },
      { input: f32(kValue.powTwo.to13), expected: f32(kValue.powTwo.to13) },
      { input: f32(kValue.powTwo.to14), expected: f32(kValue.powTwo.to14) },
      { input: f32(kValue.powTwo.to15), expected: f32(kValue.powTwo.to15) },
      { input: f32(kValue.powTwo.to16), expected: f32(kValue.powTwo.to16) },
      { input: f32(kValue.powTwo.to17), expected: f32(kValue.powTwo.to17) },
      { input: f32(kValue.powTwo.to18), expected: f32(kValue.powTwo.to18) },
      { input: f32(kValue.powTwo.to19), expected: f32(kValue.powTwo.to19) },
      { input: f32(kValue.powTwo.to20), expected: f32(kValue.powTwo.to20) },
      { input: f32(kValue.powTwo.to21), expected: f32(kValue.powTwo.to21) },
      { input: f32(kValue.powTwo.to22), expected: f32(kValue.powTwo.to22) },
      { input: f32(kValue.powTwo.to23), expected: f32(kValue.powTwo.to23) },
      { input: f32(kValue.powTwo.to24), expected: f32(kValue.powTwo.to24) },
      { input: f32(kValue.powTwo.to25), expected: f32(kValue.powTwo.to25) },
      { input: f32(kValue.powTwo.to26), expected: f32(kValue.powTwo.to26) },
      { input: f32(kValue.powTwo.to27), expected: f32(kValue.powTwo.to27) },
      { input: f32(kValue.powTwo.to28), expected: f32(kValue.powTwo.to28) },
      { input: f32(kValue.powTwo.to29), expected: f32(kValue.powTwo.to29) },
      { input: f32(kValue.powTwo.to30), expected: f32(kValue.powTwo.to30) },
      { input: f32(kValue.powTwo.to31), expected: f32(kValue.powTwo.to31) },

      // Powers of -2.0: -2.0^i: 1 <= i <= 31
      { input: f32(kValue.negPowTwo.to1), expected: f32(kValue.negPowTwo.to1) },
      { input: f32(kValue.negPowTwo.to2), expected: f32(kValue.negPowTwo.to2) },
      { input: f32(kValue.negPowTwo.to3), expected: f32(kValue.negPowTwo.to3) },
      { input: f32(kValue.negPowTwo.to4), expected: f32(kValue.negPowTwo.to4) },
      { input: f32(kValue.negPowTwo.to5), expected: f32(kValue.negPowTwo.to5) },
      { input: f32(kValue.negPowTwo.to6), expected: f32(kValue.negPowTwo.to6) },
      { input: f32(kValue.negPowTwo.to7), expected: f32(kValue.negPowTwo.to7) },
      { input: f32(kValue.negPowTwo.to8), expected: f32(kValue.negPowTwo.to8) },
      { input: f32(kValue.negPowTwo.to9), expected: f32(kValue.negPowTwo.to9) },
      { input: f32(kValue.negPowTwo.to10), expected: f32(kValue.negPowTwo.to10) },
      { input: f32(kValue.negPowTwo.to11), expected: f32(kValue.negPowTwo.to11) },
      { input: f32(kValue.negPowTwo.to12), expected: f32(kValue.negPowTwo.to12) },
      { input: f32(kValue.negPowTwo.to13), expected: f32(kValue.negPowTwo.to13) },
      { input: f32(kValue.negPowTwo.to14), expected: f32(kValue.negPowTwo.to14) },
      { input: f32(kValue.negPowTwo.to15), expected: f32(kValue.negPowTwo.to15) },
      { input: f32(kValue.negPowTwo.to16), expected: f32(kValue.negPowTwo.to16) },
      { input: f32(kValue.negPowTwo.to17), expected: f32(kValue.negPowTwo.to17) },
      { input: f32(kValue.negPowTwo.to18), expected: f32(kValue.negPowTwo.to18) },
      { input: f32(kValue.negPowTwo.to19), expected: f32(kValue.negPowTwo.to19) },
      { input: f32(kValue.negPowTwo.to20), expected: f32(kValue.negPowTwo.to20) },
      { input: f32(kValue.negPowTwo.to21), expected: f32(kValue.negPowTwo.to21) },
      { input: f32(kValue.negPowTwo.to22), expected: f32(kValue.negPowTwo.to22) },
      { input: f32(kValue.negPowTwo.to23), expected: f32(kValue.negPowTwo.to23) },
      { input: f32(kValue.negPowTwo.to24), expected: f32(kValue.negPowTwo.to24) },
      { input: f32(kValue.negPowTwo.to25), expected: f32(kValue.negPowTwo.to25) },
      { input: f32(kValue.negPowTwo.to26), expected: f32(kValue.negPowTwo.to26) },
      { input: f32(kValue.negPowTwo.to27), expected: f32(kValue.negPowTwo.to27) },
      { input: f32(kValue.negPowTwo.to28), expected: f32(kValue.negPowTwo.to28) },
      { input: f32(kValue.negPowTwo.to29), expected: f32(kValue.negPowTwo.to29) },
      { input: f32(kValue.negPowTwo.to30), expected: f32(kValue.negPowTwo.to30) },
      { input: f32(kValue.negPowTwo.to31), expected: f32(kValue.negPowTwo.to31) },
    ]);
  });
