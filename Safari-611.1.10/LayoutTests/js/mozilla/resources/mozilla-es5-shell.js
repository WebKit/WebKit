/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */


/*
 * Return true if both of these return true:
 * - LENIENT_PRED applied to CODE
 * - STRICT_PRED applied to CODE with a use strict directive added to the front
 *
 * Run STRICT_PRED first, for testing code that affects the global environment
 * in loose mode, but fails in strict mode.
 */
function testLenientAndStrict(code, lenient_pred, strict_pred) {
  return (strict_pred("'use strict'; " + code) && 
          lenient_pred(code));
}

/*
 * completesNormally(CODE) returns true if evaluating CODE (as eval
 * code) completes normally (rather than throwing an exception).
 */
function completesNormally(code) {
  try {
    eval(code);
    return true;
  } catch (exception) {
    return false;
  }
}

/*
 * returns(VALUE)(CODE) returns true if evaluating CODE (as eval code)
 * completes normally (rather than throwing an exception), yielding a value
 * strictly equal to VALUE.
 */
function returns(value) {
  return function(code) {
    shouldBe(code, JSON.stringify(value));
    return true;
  }
}

/*
 * returnsCopyOf(VALUE)(CODE) returns true if evaluating CODE (as eval code)
 * completes normally (rather than throwing an exception), yielding a value
 * that is deepEqual to VALUE.
 */
function returnsCopyOf(value) {
  return function(code) {
    try {
      return deepEqual(eval(code), value);
    } catch (exception) {
      return false;
    }
  }
}

/*
 * raisesException(EXCEPTION)(CODE) returns true if evaluating CODE (as
 * eval code) throws an exception object that is an instance of EXCEPTION,
 * and returns false if it throws any other error or evaluates
 * successfully. For example: raises(TypeError)("0()") == true.
 */
function raisesException(exception) {
  return function (code) {
    shouldThrowType(code, exception);
    return true;
  };
};

/*
 * parsesSuccessfully(CODE) returns true if CODE parses as function
 * code without an error.
 */
function parsesSuccessfully(code) {
    shouldBeTrue("!!Function("+JSON.stringify(code)+")");
    return true;
};

/*
 * parseRaisesException(EXCEPTION)(CODE) returns true if parsing CODE
 * as function code raises EXCEPTION.
 */
function parseRaisesException(exception) {
  return function (code) {
    shouldThrowType("Function("+JSON.stringify(code)+")", exception);
    return true;
  };
};

/*
 * Return the result of applying uneval to VAL, and replacing all runs
 * of whitespace with a single horizontal space (poor man's
 * tokenization).
 */
function clean_uneval(val) {
  return uneval(val).replace(/\s+/g, ' ');
}

/*
 * Return true if A is equal to B, where equality on arrays and objects
 * means that they have the same set of enumerable properties, the values
 * of each property are deep_equal, and their 'length' properties are
 * equal. Equality on other types is ==.
 */
function deepEqual(a, b) {
    if (typeof a != typeof b)
        return false;

    if (typeof a == 'object') {
        var props = {};
        // For every property of a, does b have that property with an equal value?
        for (var prop in a) {
            if (!deepEqual(a[prop], b[prop]))
                return false;
            props[prop] = true;
        }
        // Are all of b's properties present on a?
        for (var prop in b)
            if (!props[prop])
                return false;
        // length isn't enumerable, but we want to check it, too.
        return a.length == b.length;
    }

    if (a === b) {
        // Distinguish 0 from -0, even though they are ===.
        return a !== 0 || 1/a === 1/b;
    }

    // Treat NaNs as equal, even though NaN !== NaN.
    // NaNs are the only non-reflexive values, i.e., if a !== a, then a is a NaN.
    // isNaN is broken: it converts its argument to number, so isNaN("foo") => true
    return a !== a && b !== b;
}
