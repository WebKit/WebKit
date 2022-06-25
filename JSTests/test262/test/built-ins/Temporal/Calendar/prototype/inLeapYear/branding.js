// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.inleapyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const inLeapYear = Temporal.Calendar.prototype.inLeapYear;

assert.sameValue(typeof inLeapYear, "function");

const args = [new Temporal.PlainDate(2021, 3, 4)];

assert.throws(TypeError, () => inLeapYear.apply(undefined, args), "undefined");
assert.throws(TypeError, () => inLeapYear.apply(null, args), "null");
assert.throws(TypeError, () => inLeapYear.apply(true, args), "true");
assert.throws(TypeError, () => inLeapYear.apply("", args), "empty string");
assert.throws(TypeError, () => inLeapYear.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => inLeapYear.apply(1, args), "1");
assert.throws(TypeError, () => inLeapYear.apply({}, args), "plain object");
assert.throws(TypeError, () => inLeapYear.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => inLeapYear.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
