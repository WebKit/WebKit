// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const weekOfYear = Temporal.Calendar.prototype.weekOfYear;

assert.sameValue(typeof weekOfYear, "function");

const args = [new Temporal.PlainDate(2021, 7, 20)];

assert.throws(TypeError, () => weekOfYear.apply(undefined, args), "undefined");
assert.throws(TypeError, () => weekOfYear.apply(null, args), "null");
assert.throws(TypeError, () => weekOfYear.apply(true, args), "true");
assert.throws(TypeError, () => weekOfYear.apply("", args), "empty string");
assert.throws(TypeError, () => weekOfYear.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => weekOfYear.apply(1, args), "1");
assert.throws(TypeError, () => weekOfYear.apply({}, args), "plain object");
assert.throws(TypeError, () => weekOfYear.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => weekOfYear.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
