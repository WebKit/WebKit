/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the f32 arithmetic unary expression operations
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';
import { anyOf, correctlyRoundedThreshold } from '../../../../util/compare.js';
import { f32, TypeF32 } from '../../../../util/conversion.js';
import { fullF32Range, isSubnormalNumber, quantizeToF32 } from '../../../../util/math.js';
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
      if (isSubnormalNumber(f32_x)) {
        return { input: [f32(x)], expected: anyOf(f32(-f32_x), f32(0.0)) };
      } else {
        return { input: [f32(x)], expected: f32(-f32_x) };
      }
    };

    const cases = fullF32Range({ neg_norm: 250, neg_sub: 20, pos_sub: 20, pos_norm: 250 }).map(x =>
      makeCase(x)
    );

    run(t, unary('-'), [TypeF32], TypeF32, cfg, cases);
  });
