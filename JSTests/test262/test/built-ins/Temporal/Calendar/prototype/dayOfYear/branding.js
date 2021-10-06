// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dayofyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const dayOfYear = Temporal.Calendar.prototype.dayOfYear;

assert.sameValue(typeof dayOfYear, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => dayOfYear.apply(undefined, args), "undefined");
assert.throws(TypeError, () => dayOfYear.apply(null, args), "null");
assert.throws(TypeError, () => dayOfYear.apply(true, args), "true");
assert.throws(TypeError, () => dayOfYear.apply("", args), "empty string");
assert.throws(TypeError, () => dayOfYear.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => dayOfYear.apply(1, args), "1");
assert.throws(TypeError, () => dayOfYear.apply({}, args), "plain object");
assert.throws(TypeError, () => dayOfYear.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => dayOfYear.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
