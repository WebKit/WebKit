// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const dateUntil = Temporal.Calendar.prototype.dateUntil;

assert.sameValue(typeof dateUntil, "function");

const args = [new Temporal.PlainDate(2021, 7, 16), new Temporal.PlainDate(2021, 7, 17)];

assert.throws(TypeError, () => dateUntil.apply(undefined, args), "undefined");
assert.throws(TypeError, () => dateUntil.apply(null, args), "null");
assert.throws(TypeError, () => dateUntil.apply(true, args), "true");
assert.throws(TypeError, () => dateUntil.apply("", args), "empty string");
assert.throws(TypeError, () => dateUntil.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => dateUntil.apply(1, args), "1");
assert.throws(TypeError, () => dateUntil.apply({}, args), "plain object");
assert.throws(TypeError, () => dateUntil.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => dateUntil.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
