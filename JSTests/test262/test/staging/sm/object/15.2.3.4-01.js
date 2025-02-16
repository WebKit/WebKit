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
var summary =
  'Object.getOwnPropertyNames should play nicely with enumerator caching';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

for (var p in JSON);
var names = Object.getOwnPropertyNames(JSON);
assert.sameValue(names.length >= 2, true,
         "wrong number of property names?  [" + names + "]");
assert.sameValue(names.indexOf("parse") >= 0, true);
assert.sameValue(names.indexOf("stringify") >= 0, true);

/******************************************************************************/

print("All tests passed!");
