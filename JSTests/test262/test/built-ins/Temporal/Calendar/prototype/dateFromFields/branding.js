// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const dateFromFields = Temporal.Calendar.prototype.dateFromFields;

assert.sameValue(typeof dateFromFields, "function");

assert.throws(TypeError, () => dateFromFields.call(undefined), "undefined");
assert.throws(TypeError, () => dateFromFields.call(null), "null");
assert.throws(TypeError, () => dateFromFields.call(true), "true");
assert.throws(TypeError, () => dateFromFields.call(""), "empty string");
assert.throws(TypeError, () => dateFromFields.call(Symbol()), "symbol");
assert.throws(TypeError, () => dateFromFields.call(1), "1");
assert.throws(TypeError, () => dateFromFields.call({}), "plain object");
assert.throws(TypeError, () => dateFromFields.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => dateFromFields.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
