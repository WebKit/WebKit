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
var gTestfile = 'getPrototypeOf-array.js';
var BUGNUMBER = 769041;
var summary =
  "The [[Prototype]] of an object whose prototype chain contains an array " +
  "isn't that array's [[Prototype]]";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr = [];
assert.sameValue(Array.isArray(arr), true);
var objWithArrPrototype = Object.create(arr);
assert.sameValue(!Array.isArray(objWithArrPrototype), true);
assert.sameValue(Object.getPrototypeOf(objWithArrPrototype), arr);
var objWithArrGrandPrototype = Object.create(objWithArrPrototype);
assert.sameValue(!Array.isArray(objWithArrGrandPrototype), true);
assert.sameValue(Object.getPrototypeOf(objWithArrGrandPrototype), objWithArrPrototype);

/******************************************************************************/

print("Tests complete");
