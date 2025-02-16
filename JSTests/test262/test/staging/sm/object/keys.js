/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1038545;
var summary = "Coerce the argument passed to Object.keys using ToObject";
print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(() => Object.keys(), TypeError);
assertThrowsInstanceOf(() => Object.keys(undefined), TypeError);
assertThrowsInstanceOf(() => Object.keys(null), TypeError);

assert.deepEqual(Object.keys(1), []);
assert.deepEqual(Object.keys(true), []);
if (typeof Symbol === "function") {
    assert.deepEqual(Object.keys(Symbol("foo")), []);
}

assert.deepEqual(Object.keys("foo"), ["0", "1", "2"]);

