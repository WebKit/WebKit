// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const fields = Temporal.Calendar.prototype.fields;

assert.sameValue(typeof fields, "function");

assert.throws(TypeError, () => fields.call(undefined), "undefined");
assert.throws(TypeError, () => fields.call(null), "null");
assert.throws(TypeError, () => fields.call(true), "true");
assert.throws(TypeError, () => fields.call(""), "empty string");
assert.throws(TypeError, () => fields.call(Symbol()), "symbol");
assert.throws(TypeError, () => fields.call(1), "1");
assert.throws(TypeError, () => fields.call({}), "plain object");
assert.throws(TypeError, () => fields.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => fields.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
