// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const dateUntil = Temporal.Calendar.prototype.dateUntil;

assert.sameValue(typeof dateUntil, "function");

assert.throws(TypeError, () => dateUntil.call(undefined), "undefined");
assert.throws(TypeError, () => dateUntil.call(null), "null");
assert.throws(TypeError, () => dateUntil.call(true), "true");
assert.throws(TypeError, () => dateUntil.call(""), "empty string");
assert.throws(TypeError, () => dateUntil.call(Symbol()), "symbol");
assert.throws(TypeError, () => dateUntil.call(1), "1");
assert.throws(TypeError, () => dateUntil.call({}), "plain object");
assert.throws(TypeError, () => dateUntil.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => dateUntil.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
