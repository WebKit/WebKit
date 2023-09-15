// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.equals
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const equals = Temporal.TimeZone.prototype.equals;

assert.sameValue(typeof equals, "function");

const args = ["UTC"];

assert.throws(TypeError, () => equals.apply(undefined, args), "undefined");
assert.throws(TypeError, () => equals.apply(null, args), "null");
assert.throws(TypeError, () => equals.apply(true, args), "true");
assert.throws(TypeError, () => equals.apply("", args), "empty string");
assert.throws(TypeError, () => equals.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => equals.apply(1, args), "1");
assert.throws(TypeError, () => equals.apply({}, args), "plain object");
assert.throws(TypeError, () => equals.apply(Temporal.TimeZone, args), "Temporal.TimeZone");
assert.throws(TypeError, () => equals.apply(Temporal.TimeZone.prototype, args), "Temporal.TimeZone.prototype");
