/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
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

function testArrayGeneric()
{
  var global = "psych!";
  var eval = Array.map;

  var mapped = eval([directCheckCode], originalEval);
  assert.sameValue(mapped[0], true);
}

/******************************************************************************/

print("All tests passed!");
