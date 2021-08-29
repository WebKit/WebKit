// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.with
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const with_ = Temporal.PlainYearMonth.prototype.with;

assert.sameValue(typeof with_, "function");

assert.throws(TypeError, () => with_.call(undefined), "undefined");
assert.throws(TypeError, () => with_.call(null), "null");
assert.throws(TypeError, () => with_.call(true), "true");
assert.throws(TypeError, () => with_.call(""), "empty string");
assert.throws(TypeError, () => with_.call(Symbol()), "symbol");
assert.throws(TypeError, () => with_.call(1), "1");
assert.throws(TypeError, () => with_.call({}), "plain object");
assert.throws(TypeError, () => with_.call(Temporal.PlainYearMonth), "Temporal.PlainYearMonth");
assert.throws(TypeError, () => with_.call(Temporal.PlainYearMonth.prototype), "Temporal.PlainYearMonth.prototype");
