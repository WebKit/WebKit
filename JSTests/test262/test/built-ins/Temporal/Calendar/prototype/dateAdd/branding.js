// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const dateAdd = Temporal.Calendar.prototype.dateAdd;

assert.sameValue(typeof dateAdd, "function");

const args = [new Temporal.PlainDate(2000, 1, 1), new Temporal.Duration(1)];

assert.throws(TypeError, () => dateAdd.apply(undefined, args), "undefined");
assert.throws(TypeError, () => dateAdd.apply(null, args), "null");
assert.throws(TypeError, () => dateAdd.apply(true, args), "true");
assert.throws(TypeError, () => dateAdd.apply("", args), "empty string");
assert.throws(TypeError, () => dateAdd.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => dateAdd.apply(1, args), "1");
assert.throws(TypeError, () => dateAdd.apply({}, args), "plain object");
assert.throws(TypeError, () => dateAdd.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => dateAdd.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
