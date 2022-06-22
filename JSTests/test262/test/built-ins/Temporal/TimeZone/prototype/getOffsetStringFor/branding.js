// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getOffsetStringFor = Temporal.TimeZone.prototype.getOffsetStringFor;

assert.sameValue(typeof getOffsetStringFor, "function");

const args = [new Temporal.Instant(0n)];

assert.throws(TypeError, () => getOffsetStringFor.apply(undefined, args), "undefined");
assert.throws(TypeError, () => getOffsetStringFor.apply(null, args), "null");
assert.throws(TypeError, () => getOffsetStringFor.apply(true, args), "true");
assert.throws(TypeError, () => getOffsetStringFor.apply("", args), "empty string");
assert.throws(TypeError, () => getOffsetStringFor.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => getOffsetStringFor.apply(1, args), "1");
assert.throws(TypeError, () => getOffsetStringFor.apply({}, args), "plain object");
assert.throws(TypeError, () => getOffsetStringFor.apply(Temporal.TimeZone, args), "Temporal.TimeZone");
assert.throws(TypeError, () => getOffsetStringFor.apply(Temporal.TimeZone.prototype, args), "Temporal.TimeZone.prototype");
