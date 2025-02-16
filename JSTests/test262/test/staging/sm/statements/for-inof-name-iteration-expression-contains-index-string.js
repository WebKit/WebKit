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
var gTestfile = "for-inof-name-iteration-expression-contains-index-string.js";
var BUGNUMBER = 1235640;
var summary =
  "Don't assert parsing a for-in/of loop whose target is a name, where the " +
  "expression being iterated over contains a string containing an index";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function f()
{
  var x;
  for (x in "9")
    continue;
  assert.sameValue(x, "0");
}

f();

function g()
{
  "use strict";
  var x = "unset";
  for (x in arguments)
    continue;
  assert.sameValue(x, "unset");
}

g();

/******************************************************************************/

print("Tests complete");
