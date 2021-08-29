// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetime
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toZonedDateTime = Temporal.Instant.prototype.toZonedDateTime;

assert.sameValue(typeof toZonedDateTime, "function");

assert.throws(TypeError, () => toZonedDateTime.call(undefined), "undefined");
assert.throws(TypeError, () => toZonedDateTime.call(null), "null");
assert.throws(TypeError, () => toZonedDateTime.call(true), "true");
assert.throws(TypeError, () => toZonedDateTime.call(""), "empty string");
assert.throws(TypeError, () => toZonedDateTime.call(Symbol()), "symbol");
assert.throws(TypeError, () => toZonedDateTime.call(1), "1");
assert.throws(TypeError, () => toZonedDateTime.call({}), "plain object");
assert.throws(TypeError, () => toZonedDateTime.call(Temporal.Instant), "Temporal.Instant");
assert.throws(TypeError, () => toZonedDateTime.call(Temporal.Instant.prototype), "Temporal.Instant.prototype");
