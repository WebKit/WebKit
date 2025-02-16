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
var BUGNUMBER = 1062860;
var summary = "Object.isSealed() should return true when given primitive values as input";

print(BUGNUMBER + ": " + summary);
assert.sameValue(Object.isSealed(), true);
assert.sameValue(Object.isSealed(undefined), true);
assert.sameValue(Object.isSealed(null), true);
assert.sameValue(Object.isSealed(1), true);
assert.sameValue(Object.isSealed("foo"), true);
assert.sameValue(Object.isSealed(true), true);
if (typeof Symbol === "function") {
    assert.sameValue(Object.isSealed(Symbol()), true);
}

