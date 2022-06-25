// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getpossibleinstantsfor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getPossibleInstantsFor = Temporal.TimeZone.prototype.getPossibleInstantsFor;

assert.sameValue(typeof getPossibleInstantsFor, "function");

const args = [new Temporal.PlainDateTime(2022, 6, 22)];

assert.throws(TypeError, () => getPossibleInstantsFor.apply(undefined, args), "undefined");
assert.throws(TypeError, () => getPossibleInstantsFor.apply(null, args), "null");
assert.throws(TypeError, () => getPossibleInstantsFor.apply(true, args), "true");
assert.throws(TypeError, () => getPossibleInstantsFor.apply("", args), "empty string");
assert.throws(TypeError, () => getPossibleInstantsFor.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => getPossibleInstantsFor.apply(1, args), "1");
assert.throws(TypeError, () => getPossibleInstantsFor.apply({}, args), "plain object");
assert.throws(TypeError, () => getPossibleInstantsFor.apply(Temporal.TimeZone, args), "Temporal.TimeZone");
assert.throws(TypeError, () => getPossibleInstantsFor.apply(Temporal.TimeZone.prototype, args), "Temporal.TimeZone.prototype");
