// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.daysinmonth
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const daysInMonth = Temporal.Calendar.prototype.daysInMonth;

assert.sameValue(typeof daysInMonth, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => daysInMonth.apply(undefined, args), "undefined");
assert.throws(TypeError, () => daysInMonth.apply(null, args), "null");
assert.throws(TypeError, () => daysInMonth.apply(true, args), "true");
assert.throws(TypeError, () => daysInMonth.apply("", args), "empty string");
assert.throws(TypeError, () => daysInMonth.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => daysInMonth.apply(1, args), "1");
assert.throws(TypeError, () => daysInMonth.apply({}, args), "plain object");
assert.throws(TypeError, () => daysInMonth.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => daysInMonth.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
