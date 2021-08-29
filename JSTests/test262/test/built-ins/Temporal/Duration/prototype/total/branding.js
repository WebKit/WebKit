// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const total = Temporal.Duration.prototype.total;

assert.sameValue(typeof total, "function");

assert.throws(TypeError, () => total.call(undefined), "undefined");
assert.throws(TypeError, () => total.call(null), "null");
assert.throws(TypeError, () => total.call(true), "true");
assert.throws(TypeError, () => total.call(""), "empty string");
assert.throws(TypeError, () => total.call(Symbol()), "symbol");
assert.throws(TypeError, () => total.call(1), "1");
assert.throws(TypeError, () => total.call({}), "plain object");
assert.throws(TypeError, () => total.call(Temporal.Duration), "Temporal.Duration");
assert.throws(TypeError, () => total.call(Temporal.Duration.prototype), "Temporal.Duration.prototype");
