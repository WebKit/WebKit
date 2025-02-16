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

var arr = [];
Object.defineProperty(arr, "length", { value: 0, writable: false });

// Per Array's magical behavior, the value in the descriptor gets canonicalized
// *before* SameValue comparisons occur, so this shouldn't throw.
Object.defineProperty(arr, "length", { value: '' });

assert.sameValue(arr.length, 0);

/******************************************************************************/

print("Tests complete");
