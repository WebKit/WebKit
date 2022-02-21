// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.daysinyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const daysInYear = Temporal.Calendar.prototype.daysInYear;

assert.sameValue(typeof daysInYear, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => daysInYear.apply(undefined, args), "undefined");
assert.throws(TypeError, () => daysInYear.apply(null, args), "null");
assert.throws(TypeError, () => daysInYear.apply(true, args), "true");
assert.throws(TypeError, () => daysInYear.apply("", args), "empty string");
assert.throws(TypeError, () => daysInYear.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => daysInYear.apply(1, args), "1");
assert.throws(TypeError, () => daysInYear.apply({}, args), "plain object");
assert.throws(TypeError, () => daysInYear.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => daysInYear.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
