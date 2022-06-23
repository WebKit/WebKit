// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getInstantFor = Temporal.TimeZone.prototype.getInstantFor;

assert.sameValue(typeof getInstantFor, "function");

const args = [new Temporal.PlainDateTime(2022, 6, 22)];

assert.throws(TypeError, () => getInstantFor.apply(undefined, args), "undefined");
assert.throws(TypeError, () => getInstantFor.apply(null, args), "null");
assert.throws(TypeError, () => getInstantFor.apply(true, args), "true");
assert.throws(TypeError, () => getInstantFor.apply("", args), "empty string");
assert.throws(TypeError, () => getInstantFor.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => getInstantFor.apply(1, args), "1");
assert.throws(TypeError, () => getInstantFor.apply({}, args), "plain object");
assert.throws(TypeError, () => getInstantFor.apply(Temporal.TimeZone, args), "Temporal.TimeZone");
assert.throws(TypeError, () => getInstantFor.apply(Temporal.TimeZone.prototype, args), "Temporal.TimeZone.prototype");
