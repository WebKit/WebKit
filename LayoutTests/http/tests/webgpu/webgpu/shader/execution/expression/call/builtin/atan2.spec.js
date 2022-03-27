/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the 'atan2' builtin function
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { assert } from '../../../../../../common/util/util.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { ulpThreshold } from '../../../../../util/compare.js';
import { kValue } from '../../../../../util/constants.js';
import { f32, TypeF32 } from '../../../../../util/conversion.js';
import { biasedRange, linearRange } from '../../../../../util/math.js';
import { run } from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

g.test('f32')
  .uniqueId('cc85953f226ac95c')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#float-builtin-functions')
  .desc(
    `
atan2:
T is f32 or vecN<f32> atan2(e1: T ,e2: T ) -> T Returns the arc tangent of e1 over e2. Component-wise when T is a vector. (GLSLstd450Atan2)

TODO(#792): Decide what the ground-truth is for these tests. [1]
`
  )
  .params(u =>
    u
      .combine('storageClass', ['uniform', 'storage_r', 'storage_rw'])
      .combine('vectorize', [undefined, 2, 3, 4])
  )
  .fn(async t => {
    // [1]: Need to decide what the ground-truth is.
    const makeCase = (y, x) => {
      assert(x !== 0, 'atan2 is undefined for x = 0');
      return { input: [f32(y), f32(x)], expected: f32(Math.atan2(y, x)) };
    };

    let numeric_range = [];
    //  -2^32 < x <= -1, biased towards -1
    numeric_range = numeric_range.concat(biasedRange(-1.0, -(2 ** 32), 50));
    // -1 <= x < 0, linearly spread
    numeric_range = numeric_range.concat(linearRange(-1.0, kValue.f32.negative.max, 20));
    // 0 < x < -1, linearly spread
    numeric_range = numeric_range.concat(linearRange(kValue.f32.positive.min, 1.0, 20));
    // 1 <= x < 2^32, biased towards 1
    numeric_range = numeric_range.concat(biasedRange(1.0, 2 ** 32, 20));

    let cases = [];
    cases = cases.concat(numeric_range.map(x => makeCase(0.0, x)));
    numeric_range.forEach((y, y_idx) => {
      numeric_range.forEach((x, x_idx) => {
        if (x_idx >= y_idx) {
          cases = cases.concat(makeCase(y, x));
        }
      });
    });
    const cfg = t.params;
    cfg.cmpFloats = ulpThreshold(4096);
    run(t, builtin('atan2'), [TypeF32, TypeF32], TypeF32, cfg, cases);
  });
