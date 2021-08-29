// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.equals
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const equals = Temporal.PlainYearMonth.prototype.equals;

assert.sameValue(typeof equals, "function");

assert.throws(TypeError, () => equals.call(undefined), "undefined");
assert.throws(TypeError, () => equals.call(null), "null");
assert.throws(TypeError, () => equals.call(true), "true");
assert.throws(TypeError, () => equals.call(""), "empty string");
assert.throws(TypeError, () => equals.call(Symbol()), "symbol");
assert.throws(TypeError, () => equals.call(1), "1");
assert.throws(TypeError, () => equals.call({}), "plain object");
assert.throws(TypeError, () => equals.call(Temporal.PlainYearMonth), "Temporal.PlainYearMonth");
assert.throws(TypeError, () => equals.call(Temporal.PlainYearMonth.prototype), "Temporal.PlainYearMonth.prototype");
