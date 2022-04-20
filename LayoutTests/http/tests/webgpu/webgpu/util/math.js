/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../common/util/util.js';
import { kBit, kValue } from './constants.js';
import { f32, f32Bits, i32 } from './conversion.js';

/**
 * A multiple of 8 guaranteed to be way too large to allocate (just under 8 pebibytes).
 * This is a "safe" integer (ULP <= 1.0) very close to MAX_SAFE_INTEGER.
 *
 * Note: allocations of this size are likely to exceed limitations other than just the system's
 * physical memory, so test cases are also needed to try to trigger "true" OOM.
 */
export const kMaxSafeMultipleOf8 = Number.MAX_SAFE_INTEGER - 7;

/** Round `n` up to the next multiple of `alignment` (inclusive). */
// MAINTENANCE_TODO: Rename to `roundUp`
export function align(n, alignment) {
  assert(Number.isInteger(n) && n >= 0, 'n must be a non-negative integer');
  assert(Number.isInteger(alignment) && alignment > 0, 'alignment must be a positive integer');
  return Math.ceil(n / alignment) * alignment;
}

/** Round `n` down to the next multiple of `alignment` (inclusive). */
export function roundDown(n, alignment) {
  assert(Number.isInteger(n) && n >= 0, 'n must be a non-negative integer');
  assert(Number.isInteger(alignment) && alignment > 0, 'alignment must be a positive integer');
  return Math.floor(n / alignment) * alignment;
}

/** Clamp a number to the provided range. */
export function clamp(n, { min, max }) {
  assert(max >= min);
  return Math.min(Math.max(n, min), max);
}

/**
 * @returns the (absolute) Units of Last Place difference between the float32 numbers a and b, taken
 * as JS doubles. If either `a` or `b` are not finite numbers, then diffULP() returns Infinity.
 *
 * Subnormal numbers are skipped, so 0 is one ULP from the minimum normal number.
 * Subnormal values are rounded to 0.
 */
export function diffULP(a, b) {
  if (!Number.isFinite(a) || !Number.isFinite(b)) {
    return Infinity;
  }

  const arr = new Uint32Array(new Float32Array([a, b]).buffer);
  const u32_a = arr[0];
  const u32_b = arr[1];

  const sign_a = (u32_a & 0x80000000) !== 0;
  const sign_b = (u32_b & 0x80000000) !== 0;
  const masked_a = u32_a & 0x7fffffff;
  const masked_b = u32_b & 0x7fffffff;
  const subnormal_or_zero_a = (u32_a & 0x7f800000) === 0;
  const subnormal_or_zero_b = (u32_b & 0x7f800000) === 0;

  // If the number is subnormal, then reduce it to 0 for ULP comparison.
  // If the number is normal then reduce its bits-representation so to that we
  // can pretend that the subnormal numbers don't exist, for the purposes of
  // counting ULP steps from zero (or any subnormal) to any of the normal numbers.
  const bits_a = subnormal_or_zero_a ? 0 : masked_a - 0x7fffff;
  const bits_b = subnormal_or_zero_b ? 0 : masked_b - 0x7fffff;

  if (sign_a === sign_b) {
    return Math.max(bits_a, bits_b) - Math.min(bits_a, bits_b);
  }
  return bits_a + bits_b;
}

/**
 * @returns 0 if |val| is a subnormal f32 number, otherwise returns |val|
 */
function flushSubnormalNumber(val) {
  const u32_val = new Uint32Array(new Float32Array([val]).buffer)[0];
  return (u32_val & 0x7f800000) === 0 ? 0 : val;
}

/**
 * @returns 0 if |val| is a bit field for a subnormal f32 number, otherwise
 * returns |val|
 * |val| is assumed to be a u32 value representing a f32
 */
function flushSubnormalBits(val) {
  return (val & 0x7f800000) === 0 ? 0 : val;
}

/**
 * @returns 0 if |val| is a subnormal f32 number, otherwise returns |val|
 */
export function flushSubnormalScalar(val) {
  return isSubnormalScalar(val) ? f32(0) : val;
}

/**
 * @returns true if |val| is a subnormal f32 number, otherwise returns false
 * 0 is considered a non-subnormal number by this function.
 */
export function isSubnormalScalar(val) {
  if (val.type.kind !== 'f32') {
    return false;
  }

  if (val === f32(0)) {
    return false;
  }

  const u32_val = new Uint32Array(new Float32Array([val.value.valueOf()]).buffer)[0];
  return (u32_val & 0x7f800000) === 0;
}

/** Utility to pass TS numbers into |isSubnormalNumber| */
export function isSubnormalNumber(val) {
  return isSubnormalScalar(f32(val));
}

/**
 * @returns the next single precision floating point value after |val|,
 * towards +inf if |dir| is true, otherwise towards -inf.
 * If |flush| is true, all subnormal values will be flushed to 0,
 * before processing.
 * If |flush| is false, the next subnormal will be calculated when appropriate,
 * and for -/+0 the nextAfter will be the closest subnormal in the correct
 * direction.
 * |val| must be expressible as a f32.
 */
export function nextAfter(val, dir = true, flush) {
  if (Number.isNaN(val)) {
    return f32Bits(kBit.f32.nan.positive.s);
  }

  if (val === Number.POSITIVE_INFINITY) {
    return f32Bits(kBit.f32.infinity.positive);
  }

  if (val === Number.NEGATIVE_INFINITY) {
    return f32Bits(kBit.f32.infinity.negative);
  }

  val = flush ? flushSubnormalNumber(val) : val;

  // -/+0 === 0 returns true
  if (val === 0) {
    if (dir) {
      return flush ? f32Bits(kBit.f32.positive.min) : f32Bits(kBit.f32.subnormal.positive.min);
    } else {
      return flush ? f32Bits(kBit.f32.negative.max) : f32Bits(kBit.f32.subnormal.negative.max);
    }
  }

  // number is float64 internally, so need to test if value is expressible as a float32.
  const converted = new Float32Array([val])[0];
  assert(val === converted, `${val} is not expressible as a f32.`);

  const u32_val = new Uint32Array(new Float32Array([val]).buffer)[0];
  const is_positive = (u32_val & 0x80000000) === 0;
  let result = u32_val;
  if (dir === is_positive) {
    result += 1;
  } else {
    result -= 1;
  }
  result = flush ? flushSubnormalBits(result) : result;

  // Checking for overflow
  if ((result & 0x7f800000) === 0x7f800000) {
    if (dir) {
      return f32Bits(kBit.f32.infinity.positive);
    } else {
      return f32Bits(kBit.f32.infinity.negative);
    }
  }
  return f32Bits(result);
}

/**
 * @returns if a test value is correctly rounded to an target value. Only
 * defined for |test_values| being a float32. target values may be any number.
 *
 * Correctly rounded means that if the target value is precisely expressible
 * as a float32, then |test_value| === |target|.
 * Otherwise |test_value| needs to be either the closest expressible number
 * greater or less than |target|.
 *
 * By default internally tests with both subnormals being flushed to 0 and not
 * being flushed, but |accept_to_zero| and |accept_no_flush| can be used to
 * control that behaviour. At least one accept flag must be true.
 */
export function correctlyRounded(
  test_value,
  target,
  accept_to_zero = true,
  accept_no_flush = true
) {
  assert(
    accept_to_zero || accept_no_flush,
    `At least one of |accept_to_zero| & |accept_no_flush| must be true`
  );

  let result = false;
  if (accept_to_zero) {
    result = result || correctlyRoundedImpl(test_value, target, true);
  }
  if (accept_no_flush) {
    result = result || correctlyRoundedImpl(test_value, target, false);
  }
  return result;
}

function correctlyRoundedImpl(test_value, target, flush) {
  assert(test_value.type.kind === 'f32', `${test_value} is expected to be a 'f32'`);

  if (Number.isNaN(target)) {
    return Number.isNaN(test_value.value.valueOf());
  }

  if (target === Number.POSITIVE_INFINITY) {
    return test_value.value === f32Bits(kBit.f32.infinity.positive).value;
  }

  if (target === Number.NEGATIVE_INFINITY) {
    return test_value.value === f32Bits(kBit.f32.infinity.negative).value;
  }

  test_value = flush ? flushSubnormalScalar(test_value) : test_value;
  target = flush ? flushSubnormalNumber(target) : target;

  const target32 = new Float32Array([target])[0];
  const converted = target32;
  if (target === converted) {
    // expected is precisely expressible in float32
    return test_value.value === f32(target32).value;
  }

  let after_target;
  let before_target;

  if (converted > target) {
    // target32 is rounded towards +inf, so is after_target
    after_target = f32(target32);
    before_target = nextAfter(target32, false, flush);
  } else {
    // target32 is rounded towards -inf, so is before_target
    after_target = nextAfter(target32, true, flush);
    before_target = f32(target32);
  }

  return test_value.value === before_target.value || test_value.value === after_target.value;
}

/**
 * Calculates the linear interpolation between two values of a given fractional.
 *
 * If |t| is 0, |a| is returned, if |t| is 1, |b| is returned, otherwise
 * interpolation/extrapolation equivalent to a + t(b - a) is performed.
 *
 * Numerical stable version is adapted from http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0811r2.html
 */
export function lerp(a, b, t) {
  if (!Number.isFinite(a) || !Number.isFinite(b)) {
    return Number.NaN;
  }

  if ((a <= 0.0 && b >= 0.0) || (a >= 0.0 && b <= 0.0)) {
    return t * b + (1 - t) * a;
  }

  if (t === 1.0) {
    return b;
  }

  const x = a + t * (b - a);
  return t > 1.0 === b > a ? Math.max(b, x) : Math.min(b, x);
}

/** @returns a linear increasing range of numbers. */
export function linearRange(a, b, num_steps) {
  if (num_steps <= 0) {
    return Array();
  }

  // Avoid division by 0
  if (num_steps === 1) {
    return [a];
  }

  return Array.from(Array(num_steps).keys()).map(i => lerp(a, b, i / (num_steps - 1)));
}

/**
 * @returns a non-linear increasing range of numbers, with a bias towards the beginning.
 *
 * Generates a linear range on [0,1] with |num_steps|, then squares all the values to make the curve be quadratic,
 * thus biasing towards 0, but remaining on the [0, 1] range.
 * This biased range is then scaled to the desired range using lerp.
 * Different curves could be generated by changing c, where greater values of c will bias more towards 0.
 */
export function biasedRange(a, b, num_steps) {
  const c = 2;
  if (num_steps <= 0) {
    return Array();
  }

  // Avoid division by 0
  if (num_steps === 1) {
    return [a];
  }

  return Array.from(Array(num_steps).keys()).map(i =>
    lerp(a, b, Math.pow(lerp(0, 1, i / (num_steps - 1)), c))
  );
}

/**
 * @returns an ascending sorted array of numbers spread over the entire range of 32-bit floats
 *
 * Numbers are divided into 4 regions: negative normals, negative subnormals, positive subnormals & positive normals.
 * Zero is included. The normal number regions are biased towards zero, and the subnormal regions are linearly spread.
 *
 * @param counts structure param with 4 entries indicating the number of entries to be generated each region, values must be 0 or greater.
 */
export function fullF32Range(counts = { pos_sub: 10, pos_norm: 50 }) {
  counts.neg_norm = counts.neg_norm === undefined ? counts.pos_norm : counts.neg_norm;
  counts.neg_sub = counts.neg_sub === undefined ? counts.pos_sub : counts.neg_sub;
  return [
    ...biasedRange(kValue.f32.negative.max, kValue.f32.negative.min, counts.neg_norm),
    ...linearRange(
      kValue.f32.subnormal.negative.min,
      kValue.f32.subnormal.negative.max,
      counts.neg_sub
    ),

    0.0,
    ...linearRange(
      kValue.f32.subnormal.positive.min,
      kValue.f32.subnormal.positive.max,
      counts.pos_sub
    ),

    ...biasedRange(kValue.f32.positive.min, kValue.f32.positive.max, counts.pos_norm),
  ];
}

/**
 * @returns the result matrix in Array<Array<number>> type.
 *
 * Matrix multiplication. A is m x n and B is n x p. Returns
 * m x p result.
 */
// A is m x n. B is n x p. product is m x p.
export function multiplyMatrices(A, B) {
  assert(A.length > 0 && B.length > 0 && B[0].length > 0 && A[0].length === B.length);
  const product = new Array(A.length);
  for (let i = 0; i < product.length; ++i) {
    product[i] = new Array(B[0].length).fill(0);
  }

  for (let m = 0; m < A.length; ++m) {
    for (let p = 0; p < B[0].length; ++p) {
      for (let n = 0; n < B.length; ++n) {
        product[m][p] += A[m][n] * B[n][p];
      }
    }
  }

  return product;
}

/** Sign-extend the `bits`-bit number `n` to a 32-bit signed integer. */
export function signExtend(n, bits) {
  const shift = 32 - bits;
  return (n << shift) >> shift;
}

/** @returns the closest 32-bit floating point value to the input */
export function quantizeToF32(num) {
  return f32(num).value;
}

/** @returns the closest 32-bit signed integer value to the input */
export function quantizeToI32(num) {
  return i32(num).value;
}

/** @returns whether the number is an integer and a power of two */
export function isPowerOfTwo(n) {
  if (!Number.isInteger(n)) {
    return false;
  }
  return n !== 0 && (n & (n - 1)) === 0;
}

/** @returns the Greatest Common Divisor (GCD) of the inputs */
export function gcd(a, b) {
  assert(Number.isInteger(a) && a > 0);
  assert(Number.isInteger(b) && b > 0);

  while (b !== 0) {
    const bTemp = b;
    b = a % b;
    a = bTemp;
  }

  return a;
}

/** @returns the Least Common Multiplier (LCM) of the inputs */
export function lcm(a, b) {
  return (a * b) / gcd(a, b);
}
