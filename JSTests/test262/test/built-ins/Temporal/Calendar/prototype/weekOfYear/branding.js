// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const weekOfYear = Temporal.Calendar.prototype.weekOfYear;

assert.sameValue(typeof weekOfYear, "function");

assert.throws(TypeError, () => weekOfYear.call(undefined), "undefined");
assert.throws(TypeError, () => weekOfYear.call(null), "null");
assert.throws(TypeError, () => weekOfYear.call(true), "true");
assert.throws(TypeError, () => weekOfYear.call(""), "empty string");
assert.throws(TypeError, () => weekOfYear.call(Symbol()), "symbol");
assert.throws(TypeError, () => weekOfYear.call(1), "1");
assert.throws(TypeError, () => weekOfYear.call({}), "plain object");
assert.throws(TypeError, () => weekOfYear.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => weekOfYear.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
