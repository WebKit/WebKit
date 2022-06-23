// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getnexttransition
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getNextTransition = Temporal.TimeZone.prototype.getNextTransition;

assert.sameValue(typeof getNextTransition, "function");

const args = [new Temporal.Instant(0n)];

assert.throws(TypeError, () => getNextTransition.apply(undefined, args), "undefined");
assert.throws(TypeError, () => getNextTransition.apply(null, args), "null");
assert.throws(TypeError, () => getNextTransition.apply(true, args), "true");
assert.throws(TypeError, () => getNextTransition.apply("", args), "empty string");
assert.throws(TypeError, () => getNextTransition.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => getNextTransition.apply(1, args), "1");
assert.throws(TypeError, () => getNextTransition.apply({}, args), "plain object");
assert.throws(TypeError, () => getNextTransition.apply(Temporal.TimeZone, args), "Temporal.TimeZone");
assert.throws(TypeError, () => getNextTransition.apply(Temporal.TimeZone.prototype, args), "Temporal.TimeZone.prototype");
