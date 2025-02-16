/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1071464;
var summary = "Object.isFrozen() should return true when given primitive values as input";

print(BUGNUMBER + ": " + summary);
assert.sameValue(Object.isFrozen(), true);
assert.sameValue(Object.isFrozen(undefined), true);
assert.sameValue(Object.isFrozen(null), true);
assert.sameValue(Object.isFrozen(1), true);
assert.sameValue(Object.isFrozen("foo"), true);
assert.sameValue(Object.isFrozen(true), true);
if (typeof Symbol === "function") {
    assert.sameValue(Object.isFrozen(Symbol()), true);
}

