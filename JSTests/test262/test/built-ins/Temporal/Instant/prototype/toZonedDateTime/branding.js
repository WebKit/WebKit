// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetime
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toZonedDateTime = Temporal.Instant.prototype.toZonedDateTime;

assert.sameValue(typeof toZonedDateTime, "function");

const args = [{ calendar: new Temporal.Calendar("iso8601"), timeZone: new Temporal.TimeZone("UTC") }];

assert.throws(TypeError, () => toZonedDateTime.apply(undefined, args), "undefined");
assert.throws(TypeError, () => toZonedDateTime.apply(null, args), "null");
assert.throws(TypeError, () => toZonedDateTime.apply(true, args), "true");
assert.throws(TypeError, () => toZonedDateTime.apply("", args), "empty string");
assert.throws(TypeError, () => toZonedDateTime.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => toZonedDateTime.apply(1, args), "1");
assert.throws(TypeError, () => toZonedDateTime.apply({}, args), "plain object");
assert.throws(TypeError, () => toZonedDateTime.apply(Temporal.Instant, args), "Temporal.Instant");
assert.throws(TypeError, () => toZonedDateTime.apply(Temporal.Instant.prototype, args), "Temporal.Instant.prototype");
