// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindatetime.prototype.microsecond
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const microsecond = Object.getOwnPropertyDescriptor(Temporal.PlainDateTime.prototype, "microsecond").get;

assert.sameValue(typeof microsecond, "function");

assert.throws(TypeError, () => microsecond.call(undefined), "undefined");
assert.throws(TypeError, () => microsecond.call(null), "null");
assert.throws(TypeError, () => microsecond.call(true), "true");
assert.throws(TypeError, () => microsecond.call(""), "empty string");
assert.throws(TypeError, () => microsecond.call(Symbol()), "symbol");
assert.throws(TypeError, () => microsecond.call(1), "1");
assert.throws(TypeError, () => microsecond.call({}), "plain object");
assert.throws(TypeError, () => microsecond.call(Temporal.PlainDateTime), "Temporal.PlainDateTime");
assert.throws(TypeError, () => microsecond.call(Temporal.PlainDateTime.prototype), "Temporal.PlainDateTime.prototype");
