// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.calendar.prototype.id
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const descriptor = Object.getOwnPropertyDescriptor(Temporal.Calendar.prototype, "id");
const id = descriptor.get;

assert.sameValue(typeof id, "function");

assert.throws(TypeError, () => id.call(undefined), "undefined");
assert.throws(TypeError, () => id.call(null), "null");
assert.throws(TypeError, () => id.call(true), "true");
assert.throws(TypeError, () => id.call(""), "empty string");
assert.throws(TypeError, () => id.call(Symbol()), "symbol");
assert.throws(TypeError, () => id.call(1), "1");
assert.throws(TypeError, () => id.call({}), "plain object");
assert.throws(TypeError, () => id.call(Temporal.Calendar), "Temporal.Calendar");
assert.throws(TypeError, () => id.call(Temporal.Calendar.prototype), "Temporal.Calendar.prototype");
