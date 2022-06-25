// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.month
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const month = Temporal.Calendar.prototype.month;

assert.sameValue(typeof month, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => month.apply(undefined, args), "undefined");
assert.throws(TypeError, () => month.apply(null, args), "null");
assert.throws(TypeError, () => month.apply(true, args), "true");
assert.throws(TypeError, () => month.apply("", args), "empty string");
assert.throws(TypeError, () => month.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => month.apply(1, args), "1");
assert.throws(TypeError, () => month.apply({}, args), "plain object");
assert.throws(TypeError, () => month.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => month.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
