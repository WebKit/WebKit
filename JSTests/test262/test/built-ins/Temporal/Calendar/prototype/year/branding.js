// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.year
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const year = Temporal.Calendar.prototype.year;

assert.sameValue(typeof year, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => year.apply(undefined, args), "undefined");
assert.throws(TypeError, () => year.apply(null, args), "null");
assert.throws(TypeError, () => year.apply(true, args), "true");
assert.throws(TypeError, () => year.apply("", args), "empty string");
assert.throws(TypeError, () => year.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => year.apply(1, args), "1");
assert.throws(TypeError, () => year.apply({}, args), "plain object");
assert.throws(TypeError, () => year.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => year.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
