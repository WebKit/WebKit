// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetimeiso
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toZonedDateTimeISO = Temporal.Instant.prototype.toZonedDateTimeISO;

assert.sameValue(typeof toZonedDateTimeISO, "function");

assert.throws(TypeError, () => toZonedDateTimeISO.call(undefined), "undefined");
assert.throws(TypeError, () => toZonedDateTimeISO.call(null), "null");
assert.throws(TypeError, () => toZonedDateTimeISO.call(true), "true");
assert.throws(TypeError, () => toZonedDateTimeISO.call(""), "empty string");
assert.throws(TypeError, () => toZonedDateTimeISO.call(Symbol()), "symbol");
assert.throws(TypeError, () => toZonedDateTimeISO.call(1), "1");
assert.throws(TypeError, () => toZonedDateTimeISO.call({}), "plain object");
assert.throws(TypeError, () => toZonedDateTimeISO.call(Temporal.Instant), "Temporal.Instant");
assert.throws(TypeError, () => toZonedDateTimeISO.call(Temporal.Instant.prototype), "Temporal.Instant.prototype");
