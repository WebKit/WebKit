// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const yearMonthFromFields = Temporal.Calendar.prototype.yearMonthFromFields;

assert.sameValue(typeof yearMonthFromFields, "function");

const args = [{ year: 2021, month: 1 }];

assert.throws(TypeError, () => yearMonthFromFields.apply(undefined, args), "undefined");
assert.throws(TypeError, () => yearMonthFromFields.apply(null, args), "null");
assert.throws(TypeError, () => yearMonthFromFields.apply(true, args), "true");
assert.throws(TypeError, () => yearMonthFromFields.apply("", args), "empty string");
assert.throws(TypeError, () => yearMonthFromFields.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => yearMonthFromFields.apply(1, args), "1");
assert.throws(TypeError, () => yearMonthFromFields.apply({}, args), "plain object");
assert.throws(TypeError, () => yearMonthFromFields.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => yearMonthFromFields.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
