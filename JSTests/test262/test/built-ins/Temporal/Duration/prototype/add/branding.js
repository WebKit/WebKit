// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const add = Temporal.Duration.prototype.add;

assert.sameValue(typeof add, "function");

const args = [new Temporal.Duration(0, 0, 0, 0, 5)];

assert.throws(TypeError, () => add.apply(undefined, args), "undefined");
assert.throws(TypeError, () => add.apply(null, args), "null");
assert.throws(TypeError, () => add.apply(true, args), "true");
assert.throws(TypeError, () => add.apply("", args), "empty string");
assert.throws(TypeError, () => add.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => add.apply(1, args), "1");
assert.throws(TypeError, () => add.apply({}, args), "plain object");
assert.throws(TypeError, () => add.apply(Temporal.Duration, args), "Temporal.Duration");
assert.throws(TypeError, () => add.apply(Temporal.Duration.prototype, args), "Temporal.Duration.prototype");
