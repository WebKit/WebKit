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
var BUGNUMBER = 866700;
var summary = "Assertion redefining non-writable length to a non-numeric value";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var count = 0;

var convertible =
  {
    valueOf: function()
    {
      count++;
      if (count > 2)
        return 0;
      throw new SyntaxError("fnord");
    }
  };

var arr = [];
Object.defineProperty(arr, "length", { value: 0, writable: false });

try
{
  Object.defineProperty(arr, "length",
                        {
                          value: convertible,
                          writable: true,
                          configurable: true,
                          enumerable: true
                        });
  throw new Error("didn't throw");
}
catch (e)
{
  assert.sameValue(e instanceof SyntaxError, true, "expected SyntaxError, got " + e);
}

assert.sameValue(count, 1);
assert.sameValue(arr.length, 0);

/******************************************************************************/

print("Tests complete");
