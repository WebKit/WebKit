// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Collection of functions used to assert the correctness of TypedArray objects.
defines:
  - typedArrayConstructors
  - floatArrayConstructors
  - intArrayConstructors
  - TypedArray
  - testWithTypedArrayConstructors
  - testWithAtomicsFriendlyTypedArrayConstructors
  - testWithNonAtomicsFriendlyTypedArrayConstructors
  - testTypedArrayConversions
---*/

/**
 * Array containing every typed array constructor.
 */
var typedArrayConstructors = [
  Float64Array,
  Float32Array,
  Int32Array,
  Int16Array,
  Int8Array,
  Uint32Array,
  Uint16Array,
  Uint8Array,
  Uint8ClampedArray
];

var floatArrayConstructors = typedArrayConstructors.slice(0, 2);
var intArrayConstructors = typedArrayConstructors.slice(2, 7);

/**
 * The %TypedArray% intrinsic constructor function.
 */
var TypedArray = Object.getPrototypeOf(Int8Array);

/**
 * Callback for testing a typed array constructor.
 *
 * @callback typedArrayConstructorCallback
 * @param {Function} Constructor the constructor object to test with.
 */

/**
 * Calls the provided function for every typed array constructor.
 *
 * @param {typedArrayConstructorCallback} f - the function to call for each typed array constructor.
 * @param {Array} selected - An optional Array with filtered typed arrays
 */
function testWithTypedArrayConstructors(f, selected) {
  var constructors = selected || typedArrayConstructors;
  for (var i = 0; i < constructors.length; ++i) {
    var constructor = constructors[i];
    try {
      f(constructor);
    } catch (e) {
      e.message += " (Testing with " + constructor.name + ".)";
      throw e;
    }
  }
}

/**
 * Calls the provided function for every non-"Atomics Friendly" typed array constructor.
 *
 * @param {typedArrayConstructorCallback} f - the function to call for each typed array constructor.
 * @param {Array} selected - An optional Array with filtered typed arrays
 */
function testWithNonAtomicsFriendlyTypedArrayConstructors(f) {
  testWithTypedArrayConstructors(f, [
    Float64Array,
    Float32Array,
    Uint8ClampedArray
  ]);
}

/**
 * Calls the provided function for every "Atomics Friendly" typed array constructor.
 *
 * @param {typedArrayConstructorCallback} f - the function to call for each typed array constructor.
 * @param {Array} selected - An optional Array with filtered typed arrays
 */
function testWithAtomicsFriendlyTypedArrayConstructors(f) {
  testWithTypedArrayConstructors(f, [
    Int32Array,
    Int16Array,
    Int8Array,
    Uint32Array,
    Uint16Array,
    Uint8Array,
  ]);
}

/**
 * Helper for conversion operations on TypedArrays, the expected values
 * properties are indexed in order to match the respective value for each
 * TypedArray constructor
 * @param  {Function} fn - the function to call for each constructor and value.
 *                         will be called with the constructor, value, expected
 *                         value, and a initial value that can be used to avoid
 *                         a false positive with an equivalent expected value.
 */
function testTypedArrayConversions(byteConversionValues, fn) {
  var values = byteConversionValues.values;
  var expected = byteConversionValues.expected;

  testWithTypedArrayConstructors(function(TA) {
    var name = TA.name.slice(0, -5);

    return values.forEach(function(value, index) {
      var exp = expected[name][index];
      var initial = 0;
      if (exp === 0) {
        initial = 1;
      }
      fn(TA, value, exp, initial);
    });
  });
}
