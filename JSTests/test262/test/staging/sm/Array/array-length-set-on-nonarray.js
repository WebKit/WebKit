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
var BUGNUMBER = 548671;
var summary =
  "Don't use a shared-permanent inherited property to implement " +
  "[].length or (function(){}).length";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var a = [];
a.p = 1;
var x = Object.create(a);
assert.sameValue(x.length, 0);
assert.sameValue(x.p, 1);
assert.sameValue(a.length, 0);

print("All tests passed!");
