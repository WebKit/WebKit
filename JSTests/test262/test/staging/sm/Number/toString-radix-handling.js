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
var BUGNUMBER = 647385;
var summary =
  "Number.prototype.toString should use ToInteger on the radix and should " +
  "throw a RangeError if the radix is bad";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function test(r)
{
  try
  {
    5..toString(r);
    throw "should have thrown";
  }
  catch (e)
  {
    assert.sameValue(e instanceof RangeError, true, "expected a RangeError, got " + e);
  }
}
test(Math.pow(2, 32) + 10);
test(55);

/******************************************************************************/

print("All tests passed!");
