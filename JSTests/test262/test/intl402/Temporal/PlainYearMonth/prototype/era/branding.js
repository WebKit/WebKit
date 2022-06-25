// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plainyearmonth.prototype.era
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const era = Object.getOwnPropertyDescriptor(Temporal.PlainYearMonth.prototype, "era").get;

assert.sameValue(typeof era, "function");

assert.throws(TypeError, () => era.call(undefined), "undefined");
assert.throws(TypeError, () => era.call(null), "null");
assert.throws(TypeError, () => era.call(true), "true");
assert.throws(TypeError, () => era.call(""), "empty string");
assert.throws(TypeError, () => era.call(Symbol()), "symbol");
assert.throws(TypeError, () => era.call(1), "1");
assert.throws(TypeError, () => era.call({}), "plain object");
assert.throws(TypeError, () => era.call(Temporal.PlainYearMonth), "Temporal.PlainYearMonth");
assert.throws(TypeError, () => era.call(Temporal.PlainYearMonth.prototype), "Temporal.PlainYearMonth.prototype");
