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
assert.sameValue(Object.hasOwn({}, "any"), false);
assertThrowsInstanceOf(() => Object.hasOwn(null, "any"), TypeError);

var x = { test: 'test value'}
var y = {}
var z = Object.create(x);

assert.sameValue(Object.hasOwn(x, "test"), true);
assert.sameValue(Object.hasOwn(y, "test"), false);
assert.sameValue(Object.hasOwn(z, "test"), false);

