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
var BUGNUMBER = 562446;
var summary = 'ES5: Array.prototype.toLocaleString';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o;

o = { length: 2, 0: 7, 1: { toLocaleString: function() { return "baz" } } };
assert.sameValue(Array.prototype.toLocaleString.call(o), "7,baz");

o = {};
assert.sameValue(Array.prototype.toLocaleString.call(o), "");

var log = '';
arr = {length: {valueOf: function () { log += "L"; return 2; }},
      0: "x", 1: "z"};
assert.sameValue(Array.prototype.toLocaleString.call(arr), "x,z");
assert.sameValue(log, "L");

/******************************************************************************/

print("All tests passed!");
