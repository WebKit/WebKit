// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthsinyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const monthsInYear = Temporal.Calendar.prototype.monthsInYear;

assert.sameValue(typeof monthsInYear, "function");

const args = [new Temporal.PlainDate(2021, 3, 4)];

assert.throws(TypeError, () => monthsInYear.apply(undefined, args), "undefined");
assert.throws(TypeError, () => monthsInYear.apply(null, args), "null");
assert.throws(TypeError, () => monthsInYear.apply(true, args), "true");
assert.throws(TypeError, () => monthsInYear.apply("", args), "empty string");
assert.throws(TypeError, () => monthsInYear.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => monthsInYear.apply(1, args), "1");
assert.throws(TypeError, () => monthsInYear.apply({}, args), "plain object");
assert.throws(TypeError, () => monthsInYear.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => monthsInYear.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
