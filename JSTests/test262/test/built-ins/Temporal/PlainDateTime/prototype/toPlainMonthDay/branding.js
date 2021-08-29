// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.toplainmonthday
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toPlainMonthDay = Temporal.PlainDateTime.prototype.toPlainMonthDay;

assert.sameValue(typeof toPlainMonthDay, "function");

assert.throws(TypeError, () => toPlainMonthDay.call(undefined), "undefined");
assert.throws(TypeError, () => toPlainMonthDay.call(null), "null");
assert.throws(TypeError, () => toPlainMonthDay.call(true), "true");
assert.throws(TypeError, () => toPlainMonthDay.call(""), "empty string");
assert.throws(TypeError, () => toPlainMonthDay.call(Symbol()), "symbol");
assert.throws(TypeError, () => toPlainMonthDay.call(1), "1");
assert.throws(TypeError, () => toPlainMonthDay.call({}), "plain object");
assert.throws(TypeError, () => toPlainMonthDay.call(Temporal.PlainDateTime), "Temporal.PlainDateTime");
assert.throws(TypeError, () => toPlainMonthDay.call(Temporal.PlainDateTime.prototype), "Temporal.PlainDateTime.prototype");
