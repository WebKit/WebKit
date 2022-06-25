// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toZonedDateTime = Temporal.PlainTime.prototype.toZonedDateTime;

assert.sameValue(typeof toZonedDateTime, "function");

const args = [{ plainDate: "2022-05-19", timeZone: "UTC" }];

assert.throws(TypeError, () => toZonedDateTime.apply(undefined, args), "undefined");
assert.throws(TypeError, () => toZonedDateTime.apply(null, args), "null");
assert.throws(TypeError, () => toZonedDateTime.apply(true, args), "true");
assert.throws(TypeError, () => toZonedDateTime.apply("", args), "empty string");
assert.throws(TypeError, () => toZonedDateTime.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => toZonedDateTime.apply(1, args), "1");
assert.throws(TypeError, () => toZonedDateTime.apply({}, args), "plain object");
assert.throws(TypeError, () => toZonedDateTime.apply(Temporal.PlainTime, args), "Temporal.PlainTime");
assert.throws(TypeError, () => toZonedDateTime.apply(Temporal.PlainTime.prototype, args), "Temporal.PlainTime.prototype");
