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
var gTestfile = '15.2.3.4-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 492849;
var summary = 'ES5: Implement Object.preventExtensions, Object.isExtensible';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assert.sameValue(typeof Object.isExtensible, "function");
assert.sameValue(Object.isExtensible.length, 1);

var slowArray = [1, 2, 3];
slowArray.slow = 5;
var objs =
  [{}, { 1: 2 }, { a: 3 }, [], [1], [, 1], slowArray, function a(){}, /a/];

for (var i = 0, sz = objs.length; i < sz; i++)
{
  var o = objs[i];
  assert.sameValue(Object.isExtensible(o), true, "object " + i + " not extensible?");

  var o2 = Object.preventExtensions(o);
  assert.sameValue(o, o2);

  assert.sameValue(Object.isExtensible(o), false, "object " + i + " is extensible?");
}

/******************************************************************************/

print("All tests passed!");
