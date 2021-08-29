// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getprevioustransition
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getPreviousTransition = Temporal.TimeZone.prototype.getPreviousTransition;

assert.sameValue(typeof getPreviousTransition, "function");

assert.throws(TypeError, () => getPreviousTransition.call(undefined), "undefined");
assert.throws(TypeError, () => getPreviousTransition.call(null), "null");
assert.throws(TypeError, () => getPreviousTransition.call(true), "true");
assert.throws(TypeError, () => getPreviousTransition.call(""), "empty string");
assert.throws(TypeError, () => getPreviousTransition.call(Symbol()), "symbol");
assert.throws(TypeError, () => getPreviousTransition.call(1), "1");
assert.throws(TypeError, () => getPreviousTransition.call({}), "plain object");
assert.throws(TypeError, () => getPreviousTransition.call(Temporal.TimeZone), "Temporal.TimeZone");
assert.throws(TypeError, () => getPreviousTransition.call(Temporal.TimeZone.prototype), "Temporal.TimeZone.prototype");
