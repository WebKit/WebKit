/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Date-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1187233;
var summary =
  "Passing a Date object to |new Date()| should copy it, not convert it to " +
  "a primitive and create it from that.";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

Date.prototype.toString = Date.prototype.valueOf = null;
var d = new Date(new Date(8675309));
assert.sameValue(d.getTime(), 8675309);

Date.prototype.valueOf = () => 42;
d = new Date(new Date(8675309));
assert.sameValue(d.getTime(), 8675309);

var D = createNewGlobal().Date;

D.prototype.toString = D.prototype.valueOf = null;
var d = new Date(new D(3141592654));
assert.sameValue(d.getTime(), 3141592654);

D.prototype.valueOf = () => 525600;
d = new Date(new D(3141592654));
assert.sameValue(d.getTime(), 3141592654);

/******************************************************************************/

print("Tests complete");
