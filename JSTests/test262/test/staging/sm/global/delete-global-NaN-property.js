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
"use strict"

//-----------------------------------------------------------------------------
var BUGNUMBER = 649570;
var summary = "|delete window.NaN| should throw a TypeError";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var g = this, v = false;
try
{
  delete this.NaN;
  throw new Error("no exception thrown");
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true,
           "Expected a TypeError, got: " + e);
}

/******************************************************************************/

print("Tests complete");
