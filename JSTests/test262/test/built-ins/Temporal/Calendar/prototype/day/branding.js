// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.day
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const day = Temporal.Calendar.prototype.day;

assert.sameValue(typeof day, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => day.apply(undefined, args), "undefined");
assert.throws(TypeError, () => day.apply(null, args), "null");
assert.throws(TypeError, () => day.apply(true, args), "true");
assert.throws(TypeError, () => day.apply("", args), "empty string");
assert.throws(TypeError, () => day.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => day.apply(1, args), "1");
assert.throws(TypeError, () => day.apply({}, args), "plain object");
assert.throws(TypeError, () => day.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => day.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
