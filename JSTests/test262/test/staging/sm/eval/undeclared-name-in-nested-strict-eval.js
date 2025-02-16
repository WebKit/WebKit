/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
flags:
  - onlyStrict
includes: [sm/non262.js, sm/non262-shell.js]
description: |
  pending
esid: pending
---*/
"use strict";

//-----------------------------------------------------------------------------
var BUGNUMBER = 514568;
var summary =
  "Verify that we don't optimize free names to gnames in eval code that's " +
  "global, when the name refers to a binding introduced by a strict mode " +
  "eval frame";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var x = "global";
assert.sameValue(eval('var x = "eval"; eval("x")'), "eval");

/******************************************************************************/

print("Tests complete!");
