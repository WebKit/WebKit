// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.daysinweek
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const daysInWeek = Temporal.Calendar.prototype.daysInWeek;

assert.sameValue(typeof daysInWeek, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => daysInWeek.apply(undefined, args), "undefined");
assert.throws(TypeError, () => daysInWeek.apply(null, args), "null");
assert.throws(TypeError, () => daysInWeek.apply(true, args), "true");
assert.throws(TypeError, () => daysInWeek.apply("", args), "empty string");
assert.throws(TypeError, () => daysInWeek.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => daysInWeek.apply(1, args), "1");
assert.throws(TypeError, () => daysInWeek.apply({}, args), "plain object");
assert.throws(TypeError, () => daysInWeek.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => daysInWeek.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
