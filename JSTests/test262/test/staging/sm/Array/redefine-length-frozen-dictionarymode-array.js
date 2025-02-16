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
var BUGNUMBER = 880591;
var summary =
  "Assertion redefining length property of a frozen dictionary-mode array";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function convertToDictionaryMode(arr)
{
  Object.defineProperty(arr, 0, { configurable: true });
  Object.defineProperty(arr, 1, { configurable: true });
  delete arr[0];
}

var arr = [];
convertToDictionaryMode(arr);
Object.freeze(arr);
Object.defineProperty(arr, "length", {});

/******************************************************************************/

print("Tests complete");
