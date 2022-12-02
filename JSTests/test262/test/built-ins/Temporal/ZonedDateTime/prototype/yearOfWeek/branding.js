// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.yearofweek
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const yearOfWeek = Object.getOwnPropertyDescriptor(Temporal.ZonedDateTime.prototype, "yearOfWeek").get;

assert.sameValue(typeof yearOfWeek, "function");

assert.throws(TypeError, () => yearOfWeek.call(undefined), "undefined");
assert.throws(TypeError, () => yearOfWeek.call(null), "null");
assert.throws(TypeError, () => yearOfWeek.call(true), "true");
assert.throws(TypeError, () => yearOfWeek.call(""), "empty string");
assert.throws(TypeError, () => yearOfWeek.call(Symbol()), "symbol");
assert.throws(TypeError, () => yearOfWeek.call(1), "1");
assert.throws(TypeError, () => yearOfWeek.call({}), "plain object");
assert.throws(TypeError, () => yearOfWeek.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => yearOfWeek.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
