// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getnexttransition
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getNextTransition = Temporal.TimeZone.prototype.getNextTransition;

assert.sameValue(typeof getNextTransition, "function");

assert.throws(TypeError, () => getNextTransition.call(undefined), "undefined");
assert.throws(TypeError, () => getNextTransition.call(null), "null");
assert.throws(TypeError, () => getNextTransition.call(true), "true");
assert.throws(TypeError, () => getNextTransition.call(""), "empty string");
assert.throws(TypeError, () => getNextTransition.call(Symbol()), "symbol");
assert.throws(TypeError, () => getNextTransition.call(1), "1");
assert.throws(TypeError, () => getNextTransition.call({}), "plain object");
assert.throws(TypeError, () => getNextTransition.call(Temporal.TimeZone), "Temporal.TimeZone");
assert.throws(TypeError, () => getNextTransition.call(Temporal.TimeZone.prototype), "Temporal.TimeZone.prototype");
