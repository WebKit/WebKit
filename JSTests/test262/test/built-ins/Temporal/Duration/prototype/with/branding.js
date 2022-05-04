// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.with
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const with_ = Temporal.Duration.prototype.with;

assert.sameValue(typeof with_, "function");

const arg = { years: 3 };

assert.throws(TypeError, () => with_.call(undefined, arg), "undefined");
assert.throws(TypeError, () => with_.call(null, arg), "null");
assert.throws(TypeError, () => with_.call(true, arg), "true");
assert.throws(TypeError, () => with_.call("", arg), "empty string");
assert.throws(TypeError, () => with_.call(Symbol(), arg), "symbol");
assert.throws(TypeError, () => with_.call(1, arg), "1");
assert.throws(TypeError, () => with_.call({}, arg), "plain object");
assert.throws(TypeError, () => with_.call(Temporal.Duration, arg), "Temporal.Duration");
assert.throws(TypeError, () => with_.call(Temporal.Duration.prototype, arg), "Temporal.Duration.prototype");
