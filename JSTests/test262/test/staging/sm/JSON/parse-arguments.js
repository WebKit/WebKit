/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-JSON-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var gTestfile = 'parse-arguments.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 653847;
var summary = "JSON.parse handling of omitted arguments";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  var r = JSON.parse();
  throw new Error("didn't throw, returned " + r);
}
catch (e)
{
  assert.sameValue(e instanceof SyntaxError, true, "expected syntax error, got: " + e);
}

/******************************************************************************/

print("Tests complete");
