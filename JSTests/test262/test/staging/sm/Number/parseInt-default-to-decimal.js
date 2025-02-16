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
var BUGNUMBER = 886949;
var summary = "ES6 (draft May 2013) 15.7.3.9 Number.parseInt(string, radix)." +
			  " Verify that Number.parseInt defaults to decimal.";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assert.sameValue(Number.parseInt("08"), 8);
assert.sameValue(Number.parseInt("09"), 9);
assert.sameValue(Number.parseInt("014"), 14);

function strictParseInt(s) { "use strict"; return Number.parseInt(s); }

assert.sameValue(strictParseInt("08"), 8);
assert.sameValue(strictParseInt("09"), 9);
assert.sameValue(strictParseInt("014"), 14);

/******************************************************************************/

print("All tests passed!");
