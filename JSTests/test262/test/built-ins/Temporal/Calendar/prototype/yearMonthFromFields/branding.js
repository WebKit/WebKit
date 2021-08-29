// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const yearMonthFromFields = Temporal.Calendar.prototype.yearMonthFromFields;

assert.sameValue(typeof yearMonthFromFields, "function");

assert.throws(TypeError, () => yearMonthFromFields.call(undefined), "undefined");
assert.throws(TypeError, () => yearMonthFromFields.call(null), "null");
assert.throws(TypeError, () => yearMonthFromFields.call(true), "true");
assert.throws(TypeError, () => yearMonthFromFields.call(""), "empty string");
assert.throws(TypeError, () => yearMonthFromFields.call(Symbol()), "symbol");
assert.throws(TypeError, () => yearMonthFromFields.call(1), "1");
assert.throws(TypeError, () => yearMonthFromFields.call({}), "plain object");
assert.throws(TypeError, () => yearMonthFromFields.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => yearMonthFromFields.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
