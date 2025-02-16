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
var BUGNUMBER = 1073446;
var summary = "Object.preventExtensions() should return its argument with no conversion when the argument is a primitive value";

print(BUGNUMBER + ": " + summary);
assert.sameValue(Object.preventExtensions(), undefined);
assert.sameValue(Object.preventExtensions(undefined), undefined);
assert.sameValue(Object.preventExtensions(null), null);
assert.sameValue(Object.preventExtensions(1), 1);
assert.sameValue(Object.preventExtensions("foo"), "foo");
assert.sameValue(Object.preventExtensions(true), true);
if (typeof Symbol === "function") {
    assert.sameValue(Object.preventExtensions(Symbol.for("foo")), Symbol.for("foo"));
}

