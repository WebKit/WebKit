// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Compare the contents of two arrays
defines: [compareArray]
---*/

// @ts-check

function isSameValue(a, b) {
  if (a === 0 && b === 0) return 1 / a === 1 / b;
  if (a !== a && b !== b) return true;

  return a === b;
}

/**
 * @template T
 * @param {T[]} a
 * @param {T[]} b
 */
function compareArray(a, b) {
  if (b.length !== a.length) {
    return false;
  }

  for (var i = 0; i < a.length; i++) {
    if (!isSameValue(b[i], a[i])) {
      return false;
    }
  }
  return true;
}

/**
 * @template T
 * @param {T[]} actual
 * @param {T[]} expected
 * @param {string} [message]
 */
assert.compareArray = function(actual, expected, message) {
  assert(compareArray(actual, expected),
         'Expected ' + assert._formatValue(actual) + ' and ' + assert._formatValue(expected) + ' to have the same contents. ' + message);
};
