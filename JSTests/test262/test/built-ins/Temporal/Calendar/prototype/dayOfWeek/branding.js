// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dayofweek
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const dayOfWeek = Temporal.Calendar.prototype.dayOfWeek;

assert.sameValue(typeof dayOfWeek, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => dayOfWeek.apply(undefined, args), "undefined");
assert.throws(TypeError, () => dayOfWeek.apply(null, args), "null");
assert.throws(TypeError, () => dayOfWeek.apply(true, args), "true");
assert.throws(TypeError, () => dayOfWeek.apply("", args), "empty string");
assert.throws(TypeError, () => dayOfWeek.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => dayOfWeek.apply(1, args), "1");
assert.throws(TypeError, () => dayOfWeek.apply({}, args), "plain object");
assert.throws(TypeError, () => dayOfWeek.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => dayOfWeek.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
