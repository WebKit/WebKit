/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution tests for the 'distance' builtin function

S is AbstractFloat, f32, f16
T is S or vecN<S>
@const fn distance(e1: T ,e2: T ) -> f32
Returns the distance between e1 and e2 (e.g. length(e1-e2)).

`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { TypeF32, TypeVec } from '../../../../../util/conversion.js';
import { distanceInterval } from '../../../../../util/f32_interval.js';
import { fullF32Range, sparseVectorF32Range } from '../../../../../util/math.js';
import { makeCaseCache } from '../../case_cache.js';
import {
  allInputSources,
  generateBinaryToF32IntervalCases,
  generateVectorPairToF32IntervalCases,
  run,
} from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

export const d = makeCaseCache('distance', {
  f32_const: () => {
    return generateBinaryToF32IntervalCases(
      fullF32Range(),
      fullF32Range(),
      'f32-only',
      distanceInterval
    );
  },
  f32_non_const: () => {
    return generateBinaryToF32IntervalCases(
      fullF32Range(),
      fullF32Range(),
      'unfiltered',
      distanceInterval
    );
  },
  f32_vec2_const: () => {
    return generateVectorPairToF32IntervalCases(
      sparseVectorF32Range(2),
      sparseVectorF32Range(2),
      'f32-only',
      distanceInterval
    );
  },
  f32_vec2_non_const: () => {
    return generateVectorPairToF32IntervalCases(
      sparseVectorF32Range(2),
      sparseVectorF32Range(2),
      'unfiltered',
      distanceInterval
    );
  },
  f32_vec3_const: () => {
    return generateVectorPairToF32IntervalCases(
      sparseVectorF32Range(3),
      sparseVectorF32Range(3),
      'f32-only',
      distanceInterval
    );
  },
  f32_vec3_non_const: () => {
    return generateVectorPairToF32IntervalCases(
      sparseVectorF32Range(3),
      sparseVectorF32Range(3),
      'unfiltered',
      distanceInterval
    );
  },
  f32_vec4_const: () => {
    return generateVectorPairToF32IntervalCases(
      sparseVectorF32Range(4),
      sparseVectorF32Range(4),
      'f32-only',
      distanceInterval
    );
  },
  f32_vec4_non_const: () => {
    return generateVectorPairToF32IntervalCases(
      sparseVectorF32Range(4),
      sparseVectorF32Range(4),
      'unfiltered',
      distanceInterval
    );
  },
});

g.test('abstract_float')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(`abstract float tests`)
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4]))
  .unimplemented();

g.test('f32')
  .specURL('https://www.w3.org/TR/WGSL/#numeric-builtin-functions')
  .desc(`f32 tests`)
  .params(u => u.combine('inputSource', allInputSources))
  .fn(async t => {
    const cases = await d.get(t.params.inputSource === 'const' ? 'f32_const' : 'f32_non_const');
    await run(t, builtin('distance'), [TypeF32, TypeF32], TypeF32, t.params, cases);
  });

g.test('f32_vec2')
  .specURL('https://www.w3.org/TR/WGSL/#numeric-builtin-functions')
  .desc(`f32 tests using vec2s`)
  .params(u => u.combine('inputSource', allInputSources))
  .fn(async t => {
    const cases = await d.get(
      t.params.inputSource === 'const' ? 'f32_vec2_const' : 'f32_vec2_non_const'
    );

    await run(
      t,
      builtin('distance'),
      [TypeVec(2, TypeF32), TypeVec(2, TypeF32)],
      TypeF32,
      t.params,
      cases
    );
  });

g.test('f32_vec3')
  .specURL('https://www.w3.org/TR/WGSL/#numeric-builtin-functions')
  .desc(`f32 tests using vec3s`)
  .params(u => u.combine('inputSource', allInputSources))
  .fn(async t => {
    const cases = await d.get(
      t.params.inputSource === 'const' ? 'f32_vec3_const' : 'f32_vec3_non_const'
    );

    await run(
      t,
      builtin('distance'),
      [TypeVec(3, TypeF32), TypeVec(3, TypeF32)],
      TypeF32,
      t.params,
      cases
    );
  });

g.test('f32_vec4')
  .specURL('https://www.w3.org/TR/WGSL/#numeric-builtin-functions')
  .desc(`f32 tests using vec4s`)
  .params(u => u.combine('inputSource', allInputSources))
  .fn(async t => {
    const cases = await d.get(
      t.params.inputSource === 'const' ? 'f32_vec4_const' : 'f32_vec4_non_const'
    );

    await run(
      t,
      builtin('distance'),
      [TypeVec(4, TypeF32), TypeVec(4, TypeF32)],
      TypeF32,
      t.params,
      cases
    );
  });

g.test('f16')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(`f16 tests`)
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4]))
  .unimplemented();
