// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.subtract
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const subtract = Temporal.Instant.prototype.subtract;

assert.sameValue(typeof subtract, "function");

const arg = new Temporal.Duration(0, 0, 0, 0, 5);

assert.throws(TypeError, () => subtract.call(undefined, arg), "undefined");
assert.throws(TypeError, () => subtract.call(null, arg), "null");
assert.throws(TypeError, () => subtract.call(true, arg), "true");
assert.throws(TypeError, () => subtract.call("", arg), "empty string");
assert.throws(TypeError, () => subtract.call(Symbol(), arg), "symbol");
assert.throws(TypeError, () => subtract.call(1, arg), "1");
assert.throws(TypeError, () => subtract.call({}, arg), "plain object");
assert.throws(TypeError, () => subtract.call(Temporal.Instant, arg), "Temporal.Instant");
assert.throws(TypeError, () => subtract.call(Temporal.Instant.prototype, arg), "Temporal.Instant.prototype");
