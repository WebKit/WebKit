// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const round = Temporal.ZonedDateTime.prototype.round;

assert.sameValue(typeof round, "function");

assert.throws(TypeError, () => round.call(undefined), "undefined");
assert.throws(TypeError, () => round.call(null), "null");
assert.throws(TypeError, () => round.call(true), "true");
assert.throws(TypeError, () => round.call(""), "empty string");
assert.throws(TypeError, () => round.call(Symbol()), "symbol");
assert.throws(TypeError, () => round.call(1), "1");
assert.throws(TypeError, () => round.call({}), "plain object");
assert.throws(TypeError, () => round.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => round.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
