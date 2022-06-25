/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the 'fract' builtin function
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { correctlyRoundedThreshold, anyOf } from '../../../../../util/compare.js';
import { kBit } from '../../../../../util/constants.js';
import { f32, f32Bits, TypeF32 } from '../../../../../util/conversion.js';
import { run } from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

g.test('f32')
  .uniqueId('58222ecf6f963798')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
fract:
T is f32 or vecN<f32> fract(e: T ) -> T Returns the fractional bits of e (e.g. e - floor(e)). Component-wise when T is a vector. (GLSLstd450Fract)
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

    run(t, builtin('fract'), [TypeF32], TypeF32, cfg, [
      // Zeroes
      { input: f32Bits(kBit.f32.positive.zero), expected: f32(0) },
      { input: f32Bits(kBit.f32.negative.zero), expected: f32(0) },

      // Positive numbers
      { input: f32Bits(0x3dcccccd), expected: f32Bits(0x3dcccccd) }, // ~0.1 -> ~0.1
      { input: f32(0.5), expected: f32(0.5) }, // 0.5 -> 0.5
      { input: f32Bits(0x3f666666), expected: f32Bits(0x3f666666) }, // ~0.9 -> ~0.9
      { input: f32Bits(0x3f800000), expected: f32(0) }, // 1 -> 0
      { input: f32Bits(0x40000000), expected: f32(0) }, // 2 -> 0
      { input: f32Bits(0x3f8e147b), expected: f32Bits(0x3de147b0) }, // ~1.11 -> ~0.11
      { input: f32Bits(0x41200069), expected: f32Bits(0x38d20000) }, // ~10.0001 -> ~0.0001

      // Negative numbers
      { input: f32Bits(0xbdcccccd), expected: f32Bits(0x3f666666) }, // ~-0.1 -> ~0.9
      { input: f32(-0.5), expected: f32(0.5) }, // -0.5 -> 0.5
      { input: f32Bits(0xbf666666), expected: f32Bits(0x3dccccd0) }, // ~-0.9 -> ~0.1
      { input: f32Bits(0xbf800000), expected: f32(0) }, // -1 -> 0
      { input: f32Bits(0xc0000000), expected: f32(0) }, // -2 -> 0
      { input: f32Bits(0xbf8e147b), expected: f32Bits(0x3f63d70a) }, // ~-1.11 -> ~0.89
      { input: f32Bits(0xc1200419), expected: f32Bits(0x3f7fbe70) }, // ~-10.0001 -> ~0.999

      // Min and Max f32
      { input: f32Bits(kBit.f32.positive.min), expected: f32Bits(kBit.f32.positive.min) },
      { input: f32Bits(kBit.f32.positive.max), expected: f32(0) },
      // For negative numbers on the extremes, here and below, different backends disagree on if this should be 1
      // exactly vs very close to 1. I think what is occurring is that if the calculation is internally done in f32,
      // i.e. rounding/flushing each step, you end up with one value, but if all of the math is done in a higher
      // precision, and then converted at the end, you end up with a different value.
      { input: f32Bits(kBit.f32.negative.max), expected: anyOf(f32Bits(0x3f7fffff), f32(1)) },
      { input: f32Bits(kBit.f32.negative.min), expected: f32(0) },

      // Subnormal f32
      {
        input: f32Bits(kBit.f32.subnormal.positive.max),
        expected: anyOf(f32(0), f32Bits(kBit.f32.subnormal.positive.max)),
      },
      {
        input: f32Bits(kBit.f32.subnormal.positive.min),
        expected: anyOf(f32(0), f32Bits(kBit.f32.subnormal.positive.min)),
      },
      // Similar to above when these values are not immediately flushed to zero, how the back end internally calculates
      // the value will dictate if the end value is 1 or very close to 1.
      {
        input: f32Bits(kBit.f32.subnormal.negative.max),
        expected: anyOf(f32(0), f32Bits(0x3f7fffff), f32(1)),
      },
      {
        input: f32Bits(kBit.f32.subnormal.negative.min),
        expected: anyOf(f32(0), f32Bits(0x3f7fffff), f32(1)),
      },
    ]);
  });
