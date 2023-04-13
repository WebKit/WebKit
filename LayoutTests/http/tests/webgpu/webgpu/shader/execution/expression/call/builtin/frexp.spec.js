/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution tests for the 'frexp' builtin function

S is f32 or f16
T is S or vecN<S>

@const fn frexp(e: T) -> result_struct

Splits e into a significand and exponent of the form significand * 2^exponent.
Returns the result_struct for the appropriate overload.


The magnitude of the significand is in the range of [0.5, 1.0) or 0.
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { allInputSources } from '../../expression.js';

export const g = makeTestGroup(GPUTest);

g.test('scalar_f32')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(
    `
f32 tests

struct __frexp_result {
  sig : f32, // significand part
  exp : i32  // exponent part
}
`
  )
  .params(u => u.combine('inputSource', allInputSources))
  .unimplemented();

g.test('scalar_f16')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(
    `
f16 tests

struct __frexp_result_f16 {
  sig : f16, // significand part
  exp : i32  // exponent part
}
`
  )
  .params(u => u.combine('inputSource', allInputSources))
  .unimplemented();

g.test('vector_f32')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(
    `
vecN<f32>

struct __frexp_result_vecN {
  sig : vecN<f32>, // significand part
  exp : vecN<i32>  // exponent part
}
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [2, 3, 4]))
  .unimplemented();

g.test('vector_f16')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(
    `
vecN<f16>

struct __frexp_result_vecN_f16 {
  sig : vecN<f16>, // significand part
  exp : vecN<i32>  // exponent part
}
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [2, 3, 4]))
  .unimplemented();
