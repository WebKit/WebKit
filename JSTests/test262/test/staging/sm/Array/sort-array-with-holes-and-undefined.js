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
var BUGNUMBER = 664528;
var summary =
  "Sorting an array containing only holes and |undefined| should move all " +
  "|undefined| to the start of the array";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var a = [, , , undefined];
a.sort();

assert.sameValue(a.hasOwnProperty(0), true);
assert.sameValue(a[0], undefined);
assert.sameValue(a.hasOwnProperty(1), false);
assert.sameValue(a.hasOwnProperty(2), false);
assert.sameValue(a.hasOwnProperty(3), false);

/******************************************************************************/

print("Tests complete");
