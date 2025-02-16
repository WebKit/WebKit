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
var BUGNUMBER = 613492;
var summary = "ES5 15.1.2.3 parseFloat(string)";

print(BUGNUMBER + ": " + summary);

assert.sameValue(parseFloat("Infinity"), Infinity);
assert.sameValue(parseFloat("+Infinity"), Infinity);
assert.sameValue(parseFloat("-Infinity"), -Infinity);

assert.sameValue(parseFloat("inf"), NaN);
assert.sameValue(parseFloat("Inf"), NaN);
assert.sameValue(parseFloat("infinity"), NaN);

assert.sameValue(parseFloat("nan"), NaN);
assert.sameValue(parseFloat("NaN"), NaN);

print("All tests passed!");
