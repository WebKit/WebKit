/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var gTestfile = 'object-toString-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 575522;
var summary = '({}).toString.call(null) == "[object Null]", ' +
              '({}).toString.call(undefined) == "[object Undefined]", ';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var toString = Object.prototype.toString;

assert.sameValue(toString.call(null), "[object Null]");
assert.sameValue(toString.call(undefined), "[object Undefined]");

assert.sameValue(toString.call(true), "[object Boolean]");
assert.sameValue(toString.call(false), "[object Boolean]");

assert.sameValue(toString.call(0), "[object Number]");
assert.sameValue(toString.call(-0), "[object Number]");
assert.sameValue(toString.call(1), "[object Number]");
assert.sameValue(toString.call(-1), "[object Number]");
assert.sameValue(toString.call(NaN), "[object Number]");
assert.sameValue(toString.call(Infinity), "[object Number]");
assert.sameValue(toString.call(-Infinity), "[object Number]");

assert.sameValue(toString.call("foopy"), "[object String]");

assert.sameValue(toString.call({}), "[object Object]");


/******************************************************************************/

print("All tests passed!");
