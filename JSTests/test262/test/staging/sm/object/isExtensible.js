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
var BUGNUMBER = 1060873;
var summary = "Object.isExtensible() should return false when given primitive values as input";

print(BUGNUMBER + ": " + summary);
assert.sameValue(Object.isExtensible(), false);
assert.sameValue(Object.isExtensible(undefined), false);
assert.sameValue(Object.isExtensible(null), false);
assert.sameValue(Object.isExtensible(1), false);
assert.sameValue(Object.isExtensible("foo"), false);
assert.sameValue(Object.isExtensible(true), false);
if (typeof Symbol === "function") {
    assert.sameValue(Object.isExtensible(Symbol()), false);
}

