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
var BUGNUMBER = 1079188;
var summary = "Coerce the argument passed to Object.getOwnPropertyDescriptor using ToObject";
print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(), TypeError);
assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(undefined), TypeError);
assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(null), TypeError);

Object.getOwnPropertyDescriptor(1);
Object.getOwnPropertyDescriptor(true);
if (typeof Symbol === "function") {
    Object.getOwnPropertyDescriptor(Symbol("foo"));
}

assert.deepEqual(Object.getOwnPropertyDescriptor("foo", "length"), {
    value: 3,
    writable: false,
    enumerable: false,
    configurable: false
});

assert.deepEqual(Object.getOwnPropertyDescriptor("foo", 0), {
    value: "f",
    writable: false,
    enumerable: true,
    configurable: false
});

