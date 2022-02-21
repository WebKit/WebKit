// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthcode
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const monthCode = Temporal.Calendar.prototype.monthCode;

assert.sameValue(typeof monthCode, "function");

const args = [new Temporal.PlainDate(2000, 1, 1)];

assert.throws(TypeError, () => monthCode.apply(undefined, args), "undefined");
assert.throws(TypeError, () => monthCode.apply(null, args), "null");
assert.throws(TypeError, () => monthCode.apply(true, args), "true");
assert.throws(TypeError, () => monthCode.apply("", args), "empty string");
assert.throws(TypeError, () => monthCode.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => monthCode.apply(1, args), "1");
assert.throws(TypeError, () => monthCode.apply({}, args), "plain object");
assert.throws(TypeError, () => monthCode.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => monthCode.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
