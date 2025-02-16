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
var BUGNUMBER = 668024;
var summary =
  'Array.prototype.splice should define, not set, the elements of the array ' +
  'it returns';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

Object.defineProperty(Object.prototype, 2,
  {
    set: function(v)
    {
      throw new Error("setter on Object.prototype called!");
    },
    get: function() { return "fnord"; },
    enumerable: false,
    configurable: true
  });

var arr = [0, 1, 2, 3, 4, 5];
var removed = arr.splice(0, 6);

assert.sameValue(arr.length, 0);
assert.sameValue(removed.length, 6);
assert.sameValue(removed[0], 0);
assert.sameValue(removed[1], 1);
assert.sameValue(removed[2], 2);
assert.sameValue(removed[3], 3);
assert.sameValue(removed[4], 4);
assert.sameValue(removed[5], 5);

/******************************************************************************/

print("Tests complete");
