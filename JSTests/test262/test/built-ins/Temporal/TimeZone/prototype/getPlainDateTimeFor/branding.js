// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getPlainDateTimeFor = Temporal.TimeZone.prototype.getPlainDateTimeFor;

assert.sameValue(typeof getPlainDateTimeFor, "function");

const args = [new Temporal.Instant(0n)];

assert.throws(TypeError, () => getPlainDateTimeFor.apply(undefined, args), "undefined");
assert.throws(TypeError, () => getPlainDateTimeFor.apply(null, args), "null");
assert.throws(TypeError, () => getPlainDateTimeFor.apply(true, args), "true");
assert.throws(TypeError, () => getPlainDateTimeFor.apply("", args), "empty string");
assert.throws(TypeError, () => getPlainDateTimeFor.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => getPlainDateTimeFor.apply(1, args), "1");
assert.throws(TypeError, () => getPlainDateTimeFor.apply({}, args), "plain object");
assert.throws(TypeError, () => getPlainDateTimeFor.apply(Temporal.TimeZone, args), "Temporal.TimeZone");
assert.throws(TypeError, () => getPlainDateTimeFor.apply(Temporal.TimeZone.prototype, args), "Temporal.TimeZone.prototype");
