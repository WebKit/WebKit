// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const yearMonthFromFields = Temporal.Calendar.prototype.yearMonthFromFields;

assert.sameValue(typeof yearMonthFromFields, "function");

const arg = { year: 2021, month: 1 };

assert.throws(TypeError, () => yearMonthFromFields.call(undefined, arg), "undefined");
assert.throws(TypeError, () => yearMonthFromFields.call(null, arg), "null");
assert.throws(TypeError, () => yearMonthFromFields.call(true, arg), "true");
assert.throws(TypeError, () => yearMonthFromFields.call("", arg), "empty string");
assert.throws(TypeError, () => yearMonthFromFields.call(Symbol(), arg), "symbol");
assert.throws(TypeError, () => yearMonthFromFields.call(1, arg), "1");
assert.throws(TypeError, () => yearMonthFromFields.call({}, arg), "plain object");
assert.throws(TypeError, () => yearMonthFromFields.call(Temporal.Calendar, arg), "Temporal.Calendar");
assert.throws(TypeError, () => yearMonthFromFields.call(Temporal.Calendar.prototype, arg), "Temporal.Calendar.prototype");
