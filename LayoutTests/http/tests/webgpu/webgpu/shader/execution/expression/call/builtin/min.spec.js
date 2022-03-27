/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the 'min' builtin function
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { anyOf, correctlyRoundedThreshold } from '../../../../../util/compare.js';
import { kBit } from '../../../../../util/constants.js';
import {
  f32,
  f32Bits,
  i32,
  i32Bits,
  TypeF32,
  TypeI32,
  TypeU32,
  u32,
} from '../../../../../util/conversion.js';
import { isSubnormalScalar } from '../../../../../util/math.js';
import { run } from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

/** Generate set of min test cases from an ascending list of values */
function generateTestCases(test_values) {
  const cases = new Array();
  test_values.forEach((e, ei) => {
    test_values.forEach((f, fi) => {
      const precise_expected = ei <= fi ? e : f;
      const expected = isSubnormalScalar(precise_expected)
        ? anyOf(precise_expected, f32(0.0))
        : precise_expected;
      cases.push({ input: [e, f], expected });
    });
  });
  return cases;
}

g.test('u32')
  .uniqueId('29aba7ede5b93cdd')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#integer-builtin-functions')
  .desc(
    `
unsigned min:
T is u32 or vecN<u32> min(e1: T ,e2: T) -> T Returns e1 if e1 is less than e2, and e2 otherwise. Component-wise when T is a vector. (GLSLstd450UMin)

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

    // This array must be strictly increasing, since that ordering determines
    // the expected values.
    const test_values = [u32(0), u32(1), u32(2), u32(0x70000000), u32(0x80000000), u32(0xffffffff)];

    run(t, builtin('min'), [TypeU32, TypeU32], TypeU32, cfg, generateTestCases(test_values));
  });

g.test('i32')
  .uniqueId('60c8ecdf409b45fc')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#integer-builtin-functions')
  .desc(
    `
signed min:
T is i32 or vecN<i32> min(e1: T ,e2: T) -> T Returns e1 if e1 is less than e2, and e2 otherwise. Component-wise when T is a vector. (GLSLstd45SUMin)

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

    // This array must be strictly increasing, since that ordering determines
    // the expected values.
    const test_values = [
      i32Bits(0x80000000),
      i32(-2),
      i32(-1),
      i32(0),
      i32(1),
      i32(2),
      i32Bits(0x70000000),
    ];

    run(t, builtin('min'), [TypeI32, TypeI32], TypeI32, cfg, generateTestCases(test_values));
  });

g.test('f32')
  .uniqueId('53efc46faad0f380')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
min:
T is f32 or vecN<f32> min(e1: T ,e2: T ) -> T Returns e2 if e2 is less than e1, and e1 otherwise. If one operand is a NaN, the other is returned. If both operands are NaNs, a NaN is returned. Component-wise when T is a vector. (GLSLstd450NMin)

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

    // This array must be strictly increasing, since that ordering determines
    // the expected values.
    const test_values = [
      f32Bits(kBit.f32.infinity.negative),
      f32Bits(kBit.f32.negative.min),
      f32(-10.0),
      f32(-1.0),
      f32Bits(kBit.f32.negative.max),
      f32Bits(kBit.f32.subnormal.negative.min),
      f32Bits(kBit.f32.subnormal.negative.max),
      f32(0.0),
      f32Bits(kBit.f32.subnormal.positive.min),
      f32Bits(kBit.f32.subnormal.positive.max),
      f32Bits(kBit.f32.positive.min),
      f32(1.0),
      f32(10.0),
      f32Bits(kBit.f32.positive.max),
      f32Bits(kBit.f32.infinity.positive),
    ];

    run(t, builtin('min'), [TypeF32, TypeF32], TypeF32, cfg, generateTestCases(test_values));
  });
