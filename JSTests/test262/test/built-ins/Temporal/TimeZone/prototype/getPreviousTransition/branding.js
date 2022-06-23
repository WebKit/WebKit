// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getprevioustransition
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getPreviousTransition = Temporal.TimeZone.prototype.getPreviousTransition;

assert.sameValue(typeof getPreviousTransition, "function");

const args = [new Temporal.Instant(0n)];

assert.throws(TypeError, () => getPreviousTransition.apply(undefined, args), "undefined");
assert.throws(TypeError, () => getPreviousTransition.apply(null, args), "null");
assert.throws(TypeError, () => getPreviousTransition.apply(true, args), "true");
assert.throws(TypeError, () => getPreviousTransition.apply("", args), "empty string");
assert.throws(TypeError, () => getPreviousTransition.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => getPreviousTransition.apply(1, args), "1");
assert.throws(TypeError, () => getPreviousTransition.apply({}, args), "plain object");
assert.throws(TypeError, () => getPreviousTransition.apply(Temporal.TimeZone, args), "Temporal.TimeZone");
assert.throws(TypeError, () => getPreviousTransition.apply(Temporal.TimeZone.prototype, args), "Temporal.TimeZone.prototype");
