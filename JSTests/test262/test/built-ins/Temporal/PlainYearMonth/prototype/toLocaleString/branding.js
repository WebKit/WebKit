// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.tolocalestring
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toLocaleString = Temporal.PlainYearMonth.prototype.toLocaleString;

assert.sameValue(typeof toLocaleString, "function");

assert.throws(TypeError, () => toLocaleString.call(undefined), "undefined");
assert.throws(TypeError, () => toLocaleString.call(null), "null");
assert.throws(TypeError, () => toLocaleString.call(true), "true");
assert.throws(TypeError, () => toLocaleString.call(""), "empty string");
assert.throws(TypeError, () => toLocaleString.call(Symbol()), "symbol");
assert.throws(TypeError, () => toLocaleString.call(1), "1");
assert.throws(TypeError, () => toLocaleString.call({}), "plain object");
assert.throws(TypeError, () => toLocaleString.call(Temporal.PlainYearMonth), "Temporal.PlainYearMonth");
assert.throws(TypeError, () => toLocaleString.call(Temporal.PlainYearMonth.prototype), "Temporal.PlainYearMonth.prototype");
