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
var summary = 'Object.freeze([]).pop() must throw a TypeError';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  Object.freeze([]).pop();
  throw new Error("didn't throw");
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "should have thrown TypeError, instead got: " + e);
}

/******************************************************************************/

print("Tests complete");
