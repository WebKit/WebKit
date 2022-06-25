// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.nanosecond
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const nanosecond = Object.getOwnPropertyDescriptor(Temporal.ZonedDateTime.prototype, "nanosecond").get;

assert.sameValue(typeof nanosecond, "function");

assert.throws(TypeError, () => nanosecond.call(undefined), "undefined");
assert.throws(TypeError, () => nanosecond.call(null), "null");
assert.throws(TypeError, () => nanosecond.call(true), "true");
assert.throws(TypeError, () => nanosecond.call(""), "empty string");
assert.throws(TypeError, () => nanosecond.call(Symbol()), "symbol");
assert.throws(TypeError, () => nanosecond.call(1), "1");
assert.throws(TypeError, () => nanosecond.call({}), "plain object");
assert.throws(TypeError, () => nanosecond.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => nanosecond.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
