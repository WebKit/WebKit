// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const monthDayFromFields = Temporal.Calendar.prototype.monthDayFromFields;

assert.sameValue(typeof monthDayFromFields, "function");

assert.throws(TypeError, () => monthDayFromFields.call(undefined), "undefined");
assert.throws(TypeError, () => monthDayFromFields.call(null), "null");
assert.throws(TypeError, () => monthDayFromFields.call(true), "true");
assert.throws(TypeError, () => monthDayFromFields.call(""), "empty string");
assert.throws(TypeError, () => monthDayFromFields.call(Symbol()), "symbol");
assert.throws(TypeError, () => monthDayFromFields.call(1), "1");
assert.throws(TypeError, () => monthDayFromFields.call({}), "plain object");
assert.throws(TypeError, () => monthDayFromFields.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => monthDayFromFields.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
