// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const mergeFields = Temporal.Calendar.prototype.mergeFields;

assert.sameValue(typeof mergeFields, "function");

const args = [{}, {}];

assert.throws(TypeError, () => mergeFields.apply(undefined, args), "undefined");
assert.throws(TypeError, () => mergeFields.apply(null, args), "null");
assert.throws(TypeError, () => mergeFields.apply(true, args), "true");
assert.throws(TypeError, () => mergeFields.apply("", args), "empty string");
assert.throws(TypeError, () => mergeFields.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => mergeFields.apply(1, args), "1");
assert.throws(TypeError, () => mergeFields.apply({}, args), "plain object");
assert.throws(TypeError, () => mergeFields.apply(Temporal.Calendar, args), "Temporal.Calendar");
assert.throws(TypeError, () => mergeFields.apply(Temporal.Calendar.prototype, args), "Temporal.Calendar.prototype");
