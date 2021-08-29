// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const dateAdd = Temporal.Calendar.prototype.dateAdd;

assert.sameValue(typeof dateAdd, "function");

assert.throws(TypeError, () => dateAdd.call(undefined), "undefined");
assert.throws(TypeError, () => dateAdd.call(null), "null");
assert.throws(TypeError, () => dateAdd.call(true), "true");
assert.throws(TypeError, () => dateAdd.call(""), "empty string");
assert.throws(TypeError, () => dateAdd.call(Symbol()), "symbol");
assert.throws(TypeError, () => dateAdd.call(1), "1");
assert.throws(TypeError, () => dateAdd.call({}), "plain object");
assert.throws(TypeError, () => dateAdd.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => dateAdd.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
