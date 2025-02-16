/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Reflect-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Reflect.getPrototypeOf returns an object's prototype.
assert.sameValue(Reflect.getPrototypeOf({}), Object.prototype);
assert.sameValue(Reflect.getPrototypeOf(Object.prototype), null);
assert.sameValue(Reflect.getPrototypeOf(Object.create(null)), null);

var proxy = new Proxy({}, {
    getPrototypeOf(t) { return Math; }
});

assert.sameValue(Reflect.getPrototypeOf(proxy), Math);

// For more Reflect.getPrototypeOf tests, see target.js.

