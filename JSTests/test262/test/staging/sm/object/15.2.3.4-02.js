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
//-----------------------------------------------------------------------------
var BUGNUMBER = 518663;
var summary = 'Object.getOwnPropertyNames: array objects';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var a, names, expected;

function arraysEqual(a1, a2)
{
  return a1.length === a2.length &&
         a1.every(function(v, i) { return v === a2[i]; });
}


a = [0, 1, 2];

names = Object.getOwnPropertyNames(a).sort();
expected = ["0", "1", "2", "length"].sort();

a = [1, , , 7];
a.p = 2;
Object.defineProperty(a, "q", { value: 42, enumerable: false });

names = Object.getOwnPropertyNames(a).sort();
expected = ["0", "3", "p", "q", "length"].sort();
assert.sameValue(arraysEqual(names, expected), true);


a = [];

names = Object.getOwnPropertyNames(a).sort();
expected = ["length"];
assert.sameValue(arraysEqual(names, expected), true);


/******************************************************************************/

print("All tests passed!");
