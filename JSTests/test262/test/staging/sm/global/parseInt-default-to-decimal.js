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
var BUGNUMBER = 583925;
var summary =
  "parseInt should treat leading-zero inputs (with radix unspecified) as " +
  "decimal, not octal (this changed in bug 786135)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assert.sameValue(parseInt("08"), 8);
assert.sameValue(parseInt("09"), 9);
assert.sameValue(parseInt("014"), 14);

function strictParseInt(s) { "use strict"; return parseInt(s); }

assert.sameValue(strictParseInt("08"), 8);
assert.sameValue(strictParseInt("09"), 9);
assert.sameValue(strictParseInt("014"), 14);

/******************************************************************************/

print("All tests passed!");
