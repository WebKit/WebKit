// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.toplaindate
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toPlainDate = Temporal.PlainMonthDay.prototype.toPlainDate;

assert.sameValue(typeof toPlainDate, "function");

const args = [{ year: 2022 }];

assert.throws(TypeError, () => toPlainDate.apply(undefined, args), "undefined");
assert.throws(TypeError, () => toPlainDate.apply(null, args), "null");
assert.throws(TypeError, () => toPlainDate.apply(true, args), "true");
assert.throws(TypeError, () => toPlainDate.apply("", args), "empty string");
assert.throws(TypeError, () => toPlainDate.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => toPlainDate.apply(1, args), "1");
assert.throws(TypeError, () => toPlainDate.apply({}, args), "plain object");
assert.throws(TypeError, () => toPlainDate.apply(Temporal.PlainMonthDay, args), "Temporal.PlainMonthDay");
assert.throws(TypeError, () => toPlainDate.apply(Temporal.PlainMonthDay.prototype, args), "Temporal.PlainMonthDay.prototype");
