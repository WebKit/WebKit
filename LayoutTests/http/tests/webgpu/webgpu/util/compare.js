/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { Colors } from '../../common/util/colors.js';
import { f32, Scalar, Vector } from './conversion.js';
import { correctlyRounded, diffULP } from './math.js';

/** Comparison describes the result of a Comparator function. */

/**
 * @returns a FloatMatch that returns true iff the two numbers are equal to, or
 * less than the specified absolute error threshold.
 */
export function absThreshold(diff) {
  return (got, expected) => {
    if (got === expected) {
      return true;
    }
    if (!Number.isFinite(got) || !Number.isFinite(expected)) {
      return false;
    }
    return Math.abs(got - expected) <= diff;
  };
}

/**
 * @returns a FloatMatch that returns true iff the two numbers are within or
 * equal to the specified ULP threshold value.
 */
export function ulpThreshold(ulp) {
  return (got, expected) => {
    if (got === expected) {
      return true;
    }
    return diffULP(got, expected) <= ulp;
  };
}

/**
 * @returns a FloatMatch that returns true iff |expected| is a correctly round
 * to |got|.
 * |got| must be expressible as a float32.
 */
export function correctlyRoundedThreshold() {
  return (got, expected) => {
    return correctlyRounded(f32(got), expected);
  };
}

/**
 * compare() compares 'got' to 'expected', returning the Comparison information.
 * @param got the value obtained from the test
 * @param expected the expected value
 * @param cmpFloats the FloatMatch used to compare floating point values
 * @returns the comparison results
 */
export function compare(got, expected, cmpFloats) {
  {
    // Check types
    const gTy = got.type;
    const eTy = expected.type;
    if (gTy !== eTy) {
      return {
        matched: false,
        got: `${Colors.red(gTy.toString())}(${got})`,
        expected: `${Colors.red(eTy.toString())}(${expected})`,
      };
    }
  }

  if (got instanceof Scalar) {
    const g = got;
    const e = expected;
    const isFloat = g.type.kind === 'f32';
    const matched = (isFloat && cmpFloats(g.value, e.value)) || (!isFloat && g.value === e.value);
    return {
      matched,
      got: g.toString(),
      expected: matched ? Colors.green(e.toString()) : Colors.red(e.toString()),
    };
  }
  if (got instanceof Vector) {
    const gLen = got.elements.length;
    const eLen = expected.elements.length;
    let matched = gLen === eLen;
    const gElements = new Array(gLen);
    const eElements = new Array(eLen);
    for (let i = 0; i < Math.max(gLen, eLen); i++) {
      if (i < gLen && i < eLen) {
        const g = got.elements[i];
        const e = expected.elements[i];
        const cmp = compare(g, e, cmpFloats);
        matched = matched && cmp.matched;
        gElements[i] = cmp.got;
        eElements[i] = cmp.expected;
        continue;
      }
      matched = false;
      if (i < gLen) {
        gElements[i] = got.elements[i].toString();
      }
      if (i < eLen) {
        eElements[i] = expected.elements[i].toString();
      }
    }
    return {
      matched,
      got: `${got.type}(${gElements.join(', ')})`,
      expected: `${expected.type}(${eElements.join(', ')})`,
    };
  }
  throw new Error(`unhandled type '${typeof got}`);
}

/** @returns a Comparator that checks whether a test value matches any of the provided values */
export function anyOf(...values) {
  return (got, cmpFloats) => {
    const failed = [];
    for (const e of values) {
      const cmp = compare(got, e, cmpFloats);
      if (cmp.matched) {
        return cmp;
      }
      failed.push(cmp.expected);
    }
    return { matched: false, got: got.toString(), expected: failed.join(' or ') };
  };
}
