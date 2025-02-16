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
var gTestfile = "for-of-var-with-initializer.js";
var BUGNUMBER = 1164741;
var summary = "Don't assert parsing |for (var x = 3 of 42);|";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  Function("for (var x = 3 of 42);");
  throw new Error("didn't throw");
}
catch (e)
{
  assert.sameValue(e instanceof SyntaxError, true,
           "expected syntax error, got: " + e);
}

/******************************************************************************/

print("Tests complete");
