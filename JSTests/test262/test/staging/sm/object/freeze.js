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
var BUGNUMBER = 1076588;
var summary = "Object.freeze() should return its argument with no conversion when the argument is a primitive value";

print(BUGNUMBER + ": " + summary);
assert.sameValue(Object.freeze(), undefined);
assert.sameValue(Object.freeze(undefined), undefined);
assert.sameValue(Object.freeze(null), null);
assert.sameValue(Object.freeze(1), 1);
assert.sameValue(Object.freeze("foo"), "foo");
assert.sameValue(Object.freeze(true), true);
if (typeof Symbol === "function") {
    assert.sameValue(Object.freeze(Symbol.for("foo")), Symbol.for("foo"));
}

