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
var BUGNUMBER = 795745;
var summary =
  "Number.prototype.to* should throw a RangeError when passed a bad precision";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function test(method, prec)
{
  try
  {
    Number.prototype[method].call(0, prec);
    throw "should have thrown";
  }
  catch (e)
  {
    assert.sameValue(e instanceof RangeError, true,
             "expected RangeError for " + method + " with precision " + prec +
             ", got " + e);
  }
}

test("toExponential", -32);
test("toFixed", -32);
test("toPrecision", -32);

test("toExponential", 9999999);
test("toFixed", 9999999);
test("toPrecision", 9999999);

test("toPrecision", 0);

/******************************************************************************/

print("Tests complete");
