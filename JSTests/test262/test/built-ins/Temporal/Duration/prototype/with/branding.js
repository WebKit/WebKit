// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.with
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const with_ = Temporal.Duration.prototype.with;

assert.sameValue(typeof with_, "function");

assert.throws(TypeError, () => with_.call(undefined), "undefined");
assert.throws(TypeError, () => with_.call(null), "null");
assert.throws(TypeError, () => with_.call(true), "true");
assert.throws(TypeError, () => with_.call(""), "empty string");
assert.throws(TypeError, () => with_.call(Symbol()), "symbol");
assert.throws(TypeError, () => with_.call(1), "1");
assert.throws(TypeError, () => with_.call({}), "plain object");
assert.throws(TypeError, () => with_.call(Temporal.Duration), "Temporal.Duration");
assert.throws(TypeError, () => with_.call(Temporal.Duration.prototype), "Temporal.Duration.prototype");
