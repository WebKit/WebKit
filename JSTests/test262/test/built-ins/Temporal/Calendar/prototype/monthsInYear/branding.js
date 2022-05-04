// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthsinyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const monthsInYear = Temporal.Calendar.prototype.monthsInYear;

assert.sameValue(typeof monthsInYear, "function");

const arg = new Temporal.PlainDate(2021, 3, 4);

assert.throws(TypeError, () => monthsInYear.call(undefined, arg), "undefined");
assert.throws(TypeError, () => monthsInYear.call(null, arg), "null");
assert.throws(TypeError, () => monthsInYear.call(true, arg), "true");
assert.throws(TypeError, () => monthsInYear.call("", arg), "empty string");
assert.throws(TypeError, () => monthsInYear.call(Symbol(), arg), "symbol");
assert.throws(TypeError, () => monthsInYear.call(1, arg), "1");
assert.throws(TypeError, () => monthsInYear.call({}, arg), "plain object");
assert.throws(TypeError, () => monthsInYear.call(Temporal.Calendar, arg), "Temporal.Calendar");
assert.throws(TypeError, () => monthsInYear.call(Temporal.Calendar.prototype, arg), "Temporal.Calendar.prototype");
