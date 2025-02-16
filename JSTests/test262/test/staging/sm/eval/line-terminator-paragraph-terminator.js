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
var BUGNUMBER = 657367;
var summary =
  "eval via the JSON parser should parse strings containing U+2028/U+2029 " +
  "(as of <https://tc39.github.io/proposal-json-superset/>, that is)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assert.sameValue(eval('("\u2028")'), "\u2028");
assert.sameValue(eval('("\u2029")'), "\u2029");

/******************************************************************************/

print("Tests complete!");
