// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const mergeFields = Temporal.Calendar.prototype.mergeFields;

assert.sameValue(typeof mergeFields, "function");

assert.throws(TypeError, () => mergeFields.call(undefined), "undefined");
assert.throws(TypeError, () => mergeFields.call(null), "null");
assert.throws(TypeError, () => mergeFields.call(true), "true");
assert.throws(TypeError, () => mergeFields.call(""), "empty string");
assert.throws(TypeError, () => mergeFields.call(Symbol()), "symbol");
assert.throws(TypeError, () => mergeFields.call(1), "1");
assert.throws(TypeError, () => mergeFields.call({}), "plain object");
assert.throws(TypeError, () => mergeFields.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => mergeFields.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
