// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plainyearmonth.prototype.year
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const year = Object.getOwnPropertyDescriptor(Temporal.PlainYearMonth.prototype, "year").get;

assert.sameValue(typeof year, "function");

assert.throws(TypeError, () => year.call(undefined), "undefined");
assert.throws(TypeError, () => year.call(null), "null");
assert.throws(TypeError, () => year.call(true), "true");
assert.throws(TypeError, () => year.call(""), "empty string");
assert.throws(TypeError, () => year.call(Symbol()), "symbol");
assert.throws(TypeError, () => year.call(1), "1");
assert.throws(TypeError, () => year.call({}), "plain object");
assert.throws(TypeError, () => year.call(Temporal.PlainYearMonth), "Temporal.PlainYearMonth");
assert.throws(TypeError, () => year.call(Temporal.PlainYearMonth.prototype), "Temporal.PlainYearMonth.prototype");
