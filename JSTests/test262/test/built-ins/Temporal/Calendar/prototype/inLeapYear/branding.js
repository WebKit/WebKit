// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.inleapyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const inLeapYear = Temporal.Calendar.prototype.inLeapYear;

assert.sameValue(typeof inLeapYear, "function");

const arg = new Temporal.PlainDate(2021, 3, 4);

assert.throws(TypeError, () => inLeapYear.call(undefined, arg), "undefined");
assert.throws(TypeError, () => inLeapYear.call(null, arg), "null");
assert.throws(TypeError, () => inLeapYear.call(true, arg), "true");
assert.throws(TypeError, () => inLeapYear.call("", arg), "empty string");
assert.throws(TypeError, () => inLeapYear.call(Symbol(), arg), "symbol");
assert.throws(TypeError, () => inLeapYear.call(1, arg), "1");
assert.throws(TypeError, () => inLeapYear.call({}, arg), "plain object");
assert.throws(TypeError, () => inLeapYear.call(Temporal.Calendar, arg), "Temporal.Calendar");
assert.throws(TypeError, () => inLeapYear.call(Temporal.Calendar.prototype, arg), "Temporal.Calendar.prototype");
