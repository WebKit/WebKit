// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.timezone
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const timeZone = Object.getOwnPropertyDescriptor(Temporal.ZonedDateTime.prototype, "timeZone").get;

assert.sameValue(typeof timeZone, "function");

assert.throws(TypeError, () => timeZone.call(undefined), "undefined");
assert.throws(TypeError, () => timeZone.call(null), "null");
assert.throws(TypeError, () => timeZone.call(true), "true");
assert.throws(TypeError, () => timeZone.call(""), "empty string");
assert.throws(TypeError, () => timeZone.call(Symbol()), "symbol");
assert.throws(TypeError, () => timeZone.call(1), "1");
assert.throws(TypeError, () => timeZone.call({}), "plain object");
assert.throws(TypeError, () => timeZone.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => timeZone.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
