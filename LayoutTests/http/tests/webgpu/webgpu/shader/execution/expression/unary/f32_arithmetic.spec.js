/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the f32 arithmetic unary expression operations
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';
import { correctlyRoundedThreshold } from '../../../../util/compare.js';
import { kValue } from '../../../../util/constants.js';
import { f32, TypeF32 } from '../../../../util/conversion.js';
import { biasedRange, linearRange, quantizeToF32 } from '../../../../util/math.js';
import { run } from '../expression.js';

import { unary } from './unary.js';

export const g = makeTestGroup(GPUTest);

g.test('negation')
  .uniqueId('xxxxxxxxx')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: -x
Accuracy: Correctly rounded
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

    const makeCase = x => {
      const f32_x = quantizeToF32(x);
      return { input: [f32(x)], expected: f32(-f32_x) };
    };

    let cases = [];
    const numeric_range = Array();
    numeric_range.concat(biasedRange(kValue.f32.negative.max, kValue.f32.negative.min, 100));
    numeric_range.concat(
      linearRange(kValue.f32.subnormal.negative.min, kValue.f32.subnormal.negative.max, 100)
    );

    numeric_range.concat(0.0);
    numeric_range.concat(
      linearRange(kValue.f32.subnormal.positive.min, kValue.f32.subnormal.positive.max, 100)
    );

    numeric_range.concat(biasedRange(kValue.f32.positive.min, kValue.f32.positive.max, 100));
    numeric_range.forEach(x => {
      cases = cases.concat(makeCase(x));
    });

    run(t, unary('-'), [TypeF32], TypeF32, cfg, cases);
  });
