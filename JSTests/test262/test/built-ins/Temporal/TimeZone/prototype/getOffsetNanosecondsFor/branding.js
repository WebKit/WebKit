// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetnanosecondsfor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getOffsetNanosecondsFor = Temporal.TimeZone.prototype.getOffsetNanosecondsFor;

assert.sameValue(typeof getOffsetNanosecondsFor, "function");

const args = [new Temporal.Instant(0n)];

assert.throws(TypeError, () => getOffsetNanosecondsFor.apply(undefined, args), "undefined");
assert.throws(TypeError, () => getOffsetNanosecondsFor.apply(null, args), "null");
assert.throws(TypeError, () => getOffsetNanosecondsFor.apply(true, args), "true");
assert.throws(TypeError, () => getOffsetNanosecondsFor.apply("", args), "empty string");
assert.throws(TypeError, () => getOffsetNanosecondsFor.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => getOffsetNanosecondsFor.apply(1, args), "1");
assert.throws(TypeError, () => getOffsetNanosecondsFor.apply({}, args), "plain object");
assert.throws(TypeError, () => getOffsetNanosecondsFor.apply(Temporal.TimeZone, args), "Temporal.TimeZone");
assert.throws(TypeError, () => getOffsetNanosecondsFor.apply(Temporal.TimeZone.prototype, args), "Temporal.TimeZone.prototype");
