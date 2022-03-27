/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the f32 arithmetic binary expression operations
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';
import { correctlyRoundedThreshold, ulpThreshold } from '../../../../util/compare.js';
import { kValue } from '../../../../util/constants.js';
import { f32, TypeF32 } from '../../../../util/conversion.js';
import { biasedRange, linearRange, quantizeToF32 } from '../../../../util/math.js';
import { run } from '../expression.js';

import { binary } from './binary.js';

export const g = makeTestGroup(GPUTest);

/* Generates an array of numbers spread over the entire range of 32-bit floats */
function fullNumericRange() {
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
  return numeric_range;
}

g.test('addition')
  .uniqueId('xxxxxxxxx')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x + y
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

    const makeCase = (lhs, rhs) => {
      const f32_lhs = quantizeToF32(lhs);
      const f32_rhs = quantizeToF32(rhs);
      return { input: [f32(lhs), f32(rhs)], expected: f32(f32_lhs + f32_rhs) };
    };

    let cases = [];
    const numeric_range = fullNumericRange();
    numeric_range.forEach(lhs => {
      numeric_range.forEach(rhs => {
        cases = cases.concat(makeCase(lhs, rhs));
      });
    });

    run(t, binary('+'), [TypeF32, TypeF32], TypeF32, cfg, cases);
  });

g.test('subtraction')
  .uniqueId('xxxxxxxxx')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x - y
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

    const makeCase = (lhs, rhs) => {
      const f32_lhs = quantizeToF32(lhs);
      const f32_rhs = quantizeToF32(rhs);
      return { input: [f32(lhs), f32(rhs)], expected: f32(f32_lhs - f32_rhs) };
    };

    let cases = [];
    const numeric_range = fullNumericRange();
    numeric_range.forEach(lhs => {
      numeric_range.forEach(rhs => {
        cases = cases.concat(makeCase(lhs, rhs));
      });
    });

    run(t, binary('-'), [TypeF32, TypeF32], TypeF32, cfg, cases);
  });

g.test('multiplication')
  .uniqueId('xxxxxxxxx')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x * y
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

    const makeCase = (lhs, rhs) => {
      const f32_lhs = quantizeToF32(lhs);
      const f32_rhs = quantizeToF32(rhs);
      return { input: [f32(lhs), f32(rhs)], expected: f32(f32_lhs * f32_rhs) };
    };

    let cases = [];
    const numeric_range = fullNumericRange();
    numeric_range.forEach(lhs => {
      numeric_range.forEach(rhs => {
        cases = cases.concat(makeCase(lhs, rhs));
      });
    });

    run(t, binary('*'), [TypeF32, TypeF32], TypeF32, cfg, cases);
  });

g.test('division')
  .uniqueId('xxxxxxxxx')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x / y
Accuracy: 2.5 ULP for |y| in the range [2^-126, 2^126]
`
  )
  .params(u =>
    u
      .combine('storageClass', ['uniform', 'storage_r', 'storage_rw'])
      .combine('vectorize', [undefined, 2, 3, 4])
  )
  .fn(async t => {
    const cfg = t.params;
    cfg.cmpFloats = ulpThreshold(2.5);

    const makeCase = (lhs, rhs) => {
      const f32_lhs = quantizeToF32(lhs);
      const f32_rhs = quantizeToF32(rhs);
      return { input: [f32(lhs), f32(rhs)], expected: f32(f32_lhs / f32_rhs) };
    };

    let cases = [];
    const lhs_numeric_range = fullNumericRange();
    const rhs_numeric_range = biasedRange(2 ** -126, 2 ** 126, 200);
    lhs_numeric_range.forEach(lhs => {
      rhs_numeric_range.forEach(rhs => {
        cases = cases.concat(makeCase(lhs, rhs));
      });
    });

    run(t, binary('/'), [TypeF32, TypeF32], TypeF32, cfg, cases);
  });

// Will be implemented as part larger derived accuracy task
g.test('modulus')
  .uniqueId('xxxxxxxxx')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x % y
Accuracy: Derived from x - y * trunc(x/y)
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();
