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
var BUGNUMBER = 1003764;
var summary = "ES6 (draft Draft May 22, 2014) ES6 20.1.2.5 Number.isSafeInteger(number)";

print(BUGNUMBER + ": " + summary);

assert.sameValue(Number.isSafeInteger.length, 1);

assert.sameValue(Number.isSafeInteger({}), false);
assert.sameValue(Number.isSafeInteger(NaN), false);
assert.sameValue(Number.isSafeInteger(+Infinity), false);
assert.sameValue(Number.isSafeInteger(-Infinity), false);

assert.sameValue(Number.isSafeInteger(-1), true);
assert.sameValue(Number.isSafeInteger(+0), true);
assert.sameValue(Number.isSafeInteger(-0), true);
assert.sameValue(Number.isSafeInteger(1), true);

assert.sameValue(Number.isSafeInteger(3.2), false);

assert.sameValue(Number.isSafeInteger(Math.pow(2, 53) - 2), true);
assert.sameValue(Number.isSafeInteger(Math.pow(2, 53) - 1), true);
assert.sameValue(Number.isSafeInteger(Math.pow(2, 53)), false);
assert.sameValue(Number.isSafeInteger(Math.pow(2, 53) + 1), false);
assert.sameValue(Number.isSafeInteger(Math.pow(2, 53) + 2), false);

assert.sameValue(Number.isSafeInteger(-Math.pow(2, 53) - 2), false);
assert.sameValue(Number.isSafeInteger(-Math.pow(2, 53) - 1), false);
assert.sameValue(Number.isSafeInteger(-Math.pow(2, 53)), false);
assert.sameValue(Number.isSafeInteger(-Math.pow(2, 53) + 1), true);
assert.sameValue(Number.isSafeInteger(-Math.pow(2, 53) + 2), true);


print("All tests passed!");
