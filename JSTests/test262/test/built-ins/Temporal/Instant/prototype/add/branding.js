// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.add
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const add = Temporal.Instant.prototype.add;

assert.sameValue(typeof add, "function");

const arg = new Temporal.Duration(0, 0, 0, 0, 5);

assert.throws(TypeError, () => add.call(undefined, arg), "undefined");
assert.throws(TypeError, () => add.call(null, arg), "null");
assert.throws(TypeError, () => add.call(true, arg), "true");
assert.throws(TypeError, () => add.call("", arg), "empty string");
assert.throws(TypeError, () => add.call(Symbol(), arg), "symbol");
assert.throws(TypeError, () => add.call(1, arg), "1");
assert.throws(TypeError, () => add.call({}, arg), "plain object");
assert.throws(TypeError, () => add.call(Temporal.Instant, arg), "Temporal.Instant");
assert.throws(TypeError, () => add.call(Temporal.Instant.prototype, arg), "Temporal.Instant.prototype");
