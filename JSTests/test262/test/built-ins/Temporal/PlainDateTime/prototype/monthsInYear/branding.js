// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindatetime.prototype.monthsinyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const monthsInYear = Object.getOwnPropertyDescriptor(Temporal.PlainDateTime.prototype, "monthsInYear").get;

assert.sameValue(typeof monthsInYear, "function");

assert.throws(TypeError, () => monthsInYear.call(undefined), "undefined");
assert.throws(TypeError, () => monthsInYear.call(null), "null");
assert.throws(TypeError, () => monthsInYear.call(true), "true");
assert.throws(TypeError, () => monthsInYear.call(""), "empty string");
assert.throws(TypeError, () => monthsInYear.call(Symbol()), "symbol");
assert.throws(TypeError, () => monthsInYear.call(1), "1");
assert.throws(TypeError, () => monthsInYear.call({}), "plain object");
assert.throws(TypeError, () => monthsInYear.call(Temporal.PlainDateTime), "Temporal.PlainDateTime");
assert.throws(TypeError, () => monthsInYear.call(Temporal.PlainDateTime.prototype), "Temporal.PlainDateTime.prototype");
