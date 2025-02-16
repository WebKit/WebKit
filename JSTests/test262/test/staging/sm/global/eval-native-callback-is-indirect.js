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
var BUGNUMBER = 604504;
var summary = "eval called from a native function is indirect";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var originalEval = eval;

var global = this;
var directCheckCode = "this === global";

function testBound()
{
  var global = "psych!";
  var eval = originalEval.bind(undefined, directCheckCode);
  assert.sameValue(eval(), true);
}
testBound();

/******************************************************************************/

print("All tests passed!");
