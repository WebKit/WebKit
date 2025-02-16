/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 501739;
var summary =
  "String.prototype.match should zero the .lastIndex when called with a " +
  "global RegExp";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var s = '0x2x4x6x8';
var p = /x/g;
p.lastIndex = 3;

var arr = s.match(p);
assert.sameValue(arr.length, 4);
arr.forEach(function(v) { assert.sameValue(v, "x"); });
assert.sameValue(p.lastIndex, 0);

/******************************************************************************/

print("Tests complete");
