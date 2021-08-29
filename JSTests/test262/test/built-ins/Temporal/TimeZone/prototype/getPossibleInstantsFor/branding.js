// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getpossibleinstantsfor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getPossibleInstantsFor = Temporal.TimeZone.prototype.getPossibleInstantsFor;

assert.sameValue(typeof getPossibleInstantsFor, "function");

assert.throws(TypeError, () => getPossibleInstantsFor.call(undefined), "undefined");
assert.throws(TypeError, () => getPossibleInstantsFor.call(null), "null");
assert.throws(TypeError, () => getPossibleInstantsFor.call(true), "true");
assert.throws(TypeError, () => getPossibleInstantsFor.call(""), "empty string");
assert.throws(TypeError, () => getPossibleInstantsFor.call(Symbol()), "symbol");
assert.throws(TypeError, () => getPossibleInstantsFor.call(1), "1");
assert.throws(TypeError, () => getPossibleInstantsFor.call({}), "plain object");
assert.throws(TypeError, () => getPossibleInstantsFor.call(Temporal.TimeZone), "Temporal.TimeZone");
assert.throws(TypeError, () => getPossibleInstantsFor.call(Temporal.TimeZone.prototype), "Temporal.TimeZone.prototype");
