// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const dateFromFields = Temporal.Calendar.prototype.dateFromFields;

assert.sameValue(typeof dateFromFields, "function");

const args = [{ year: 2000, month: 1, day: 1 }];

assert.throws(TypeError, () => dateFromFields.apply(undefined, args), "undefined");
assert.throws(TypeError, () => dateFromFields.apply(null, args), "null");
assert.throws(TypeError, () => dateFromFields.apply(true, args), "true");
assert.throws(TypeError, () => dateFromFields.apply("", args), "empty string");
assert.throws(TypeError, () => dateFromFields.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => dateFromFields.apply(1, args), "1");
assert.throws(TypeError, () => dateFromFields.apply({}, args), "plain object");
assert.throws(TypeError, () => dateFromFields.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => dateFromFields.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
