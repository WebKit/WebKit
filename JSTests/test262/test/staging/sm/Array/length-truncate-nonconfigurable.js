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
var BUGNUMBER = 858381;
var summary =
  "Array length redefinition behavior with non-configurable elements";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr = [0, 1, 2];
Object.defineProperty(arr, 1, { configurable: false });

try
{
  Object.defineProperty(arr, "length", { value: 0, writable: false });
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "must throw TypeError when array truncation would have to remove " +
           "non-configurable elements");
}

assert.sameValue(arr.length, 2, "length is highest remaining index plus one");

var desc = Object.getOwnPropertyDescriptor(arr, "length");
assert.sameValue(desc !== undefined, true);

assert.sameValue(desc.value, 2);
assert.sameValue(desc.writable, false);
assert.sameValue(desc.enumerable, false);
assert.sameValue(desc.configurable, false);

/******************************************************************************/

print("Tests complete");
