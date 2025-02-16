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
      return 0;
    }
  };

var arr = [];
Object.defineProperty(arr, "length", { value: 0, writable: false });

Object.defineProperty(arr, "length", { value: convertible });
assert.sameValue(count, 2);

Object.defineProperty(arr, "length", { value: convertible });
assert.sameValue(count, 4);

assert.sameValue(arr.length, 0);

/******************************************************************************/

print("Tests complete");
