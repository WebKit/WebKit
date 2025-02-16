/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 885798;
var summary = "ES6 (draft May 2013) 15.7.3.7 Number.EPSILON";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// Test value
assert.sameValue(Number.EPSILON, Math.pow(2, -52));

// Test property attributes
var descriptor = Object.getOwnPropertyDescriptor(Number, 'EPSILON');
assert.sameValue(descriptor.writable, false);
assert.sameValue(descriptor.configurable, false);
assert.sameValue(descriptor.enumerable, false);

