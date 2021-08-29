// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.daysinyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const daysInYear = Temporal.Calendar.prototype.daysInYear;

assert.sameValue(typeof daysInYear, "function");

assert.throws(TypeError, () => daysInYear.call(undefined), "undefined");
assert.throws(TypeError, () => daysInYear.call(null), "null");
assert.throws(TypeError, () => daysInYear.call(true), "true");
assert.throws(TypeError, () => daysInYear.call(""), "empty string");
assert.throws(TypeError, () => daysInYear.call(Symbol()), "symbol");
assert.throws(TypeError, () => daysInYear.call(1), "1");
assert.throws(TypeError, () => daysInYear.call({}), "plain object");
assert.throws(TypeError, () => daysInYear.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => daysInYear.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
