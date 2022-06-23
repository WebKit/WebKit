// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const fields = Temporal.Calendar.prototype.fields;

assert.sameValue(typeof fields, "function");

const args = [[]];

assert.throws(TypeError, () => fields.apply(undefined, args), "undefined");
assert.throws(TypeError, () => fields.apply(null, args), "null");
assert.throws(TypeError, () => fields.apply(true, args), "true");
assert.throws(TypeError, () => fields.apply("", args), "empty string");
assert.throws(TypeError, () => fields.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => fields.apply(1, args), "1");
assert.throws(TypeError, () => fields.apply({}, args), "plain object");
assert.throws(TypeError, () => fields.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => fields.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
