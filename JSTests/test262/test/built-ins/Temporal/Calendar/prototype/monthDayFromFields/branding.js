// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const monthDayFromFields = Temporal.Calendar.prototype.monthDayFromFields;

assert.sameValue(typeof monthDayFromFields, "function");

const args = [{ monthCode: "M01", day: 1 }];

assert.throws(TypeError, () => monthDayFromFields.apply(undefined, args), "undefined");
assert.throws(TypeError, () => monthDayFromFields.apply(null, args), "null");
assert.throws(TypeError, () => monthDayFromFields.apply(true, args), "true");
assert.throws(TypeError, () => monthDayFromFields.apply("", args), "empty string");
assert.throws(TypeError, () => monthDayFromFields.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => monthDayFromFields.apply(1, args), "1");
assert.throws(TypeError, () => monthDayFromFields.apply({}, args), "plain object");
assert.throws(TypeError, () => monthDayFromFields.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => monthDayFromFields.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
