// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const weekOfYear = Temporal.Calendar.prototype.weekOfYear;

assert.sameValue(typeof weekOfYear, "function");

const arg = new Temporal.PlainDate(2021, 7, 20);

assert.throws(TypeError, () => weekOfYear.call(undefined, arg), "undefined");
assert.throws(TypeError, () => weekOfYear.call(null, arg), "null");
assert.throws(TypeError, () => weekOfYear.call(true, arg), "true");
assert.throws(TypeError, () => weekOfYear.call("", arg), "empty string");
assert.throws(TypeError, () => weekOfYear.call(Symbol(), arg), "symbol");
assert.throws(TypeError, () => weekOfYear.call(1, arg), "1");
assert.throws(TypeError, () => weekOfYear.call({}, arg), "plain object");
assert.throws(TypeError, () => weekOfYear.call(Temporal.Calendar, arg), "Temporal.Calendar");
assert.throws(TypeError, () => weekOfYear.call(Temporal.Calendar.prototype, arg), "Temporal.Calendar.prototype");
