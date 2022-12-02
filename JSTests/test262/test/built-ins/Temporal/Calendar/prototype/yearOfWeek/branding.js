// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearofweek
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const yearOfWeek = Temporal.Calendar.prototype.yearOfWeek;

assert.sameValue(typeof yearOfWeek, "function");

const args = [new Temporal.PlainDate(2021, 7, 20)];

assert.throws(TypeError, () => yearOfWeek.apply(undefined, args), "undefined");
assert.throws(TypeError, () => yearOfWeek.apply(null, args), "null");
assert.throws(TypeError, () => yearOfWeek.apply(true, args), "true");
assert.throws(TypeError, () => yearOfWeek.apply("", args), "empty string");
assert.throws(TypeError, () => yearOfWeek.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => yearOfWeek.apply(1, args), "1");
assert.throws(TypeError, () => yearOfWeek.apply({}, args), "plain object");
assert.throws(TypeError, () => yearOfWeek.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => yearOfWeek.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
