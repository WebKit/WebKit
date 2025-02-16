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
var gTestfile = "if-constant-folding.js";
var BUGNUMBER = 1183400;
var summary =
  "Don't crash constant-folding an |if| governed by a truthy constant, whose " +
  "alternative statement is another |if|";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// Perform |if| constant folding correctly when the condition is constantly
// truthy and the alternative statement is another |if|.
if (true)
{
  assert.sameValue(true, true, "sanity");
}
else if (42)
{
  assert.sameValue(false, true, "not reached");
  assert.sameValue(true, false, "also not reached");
}


/******************************************************************************/

print("Tests complete");
