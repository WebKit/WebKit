// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.getisofields
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getISOFields = Temporal.PlainMonthDay.prototype.getISOFields;

assert.sameValue(typeof getISOFields, "function");

assert.throws(TypeError, () => getISOFields.call(undefined), "undefined");
assert.throws(TypeError, () => getISOFields.call(null), "null");
assert.throws(TypeError, () => getISOFields.call(true), "true");
assert.throws(TypeError, () => getISOFields.call(""), "empty string");
assert.throws(TypeError, () => getISOFields.call(Symbol()), "symbol");
assert.throws(TypeError, () => getISOFields.call(1), "1");
assert.throws(TypeError, () => getISOFields.call({}), "plain object");
assert.throws(TypeError, () => getISOFields.call(Temporal.PlainMonthDay), "Temporal.PlainMonthDay");
assert.throws(TypeError, () => getISOFields.call(Temporal.PlainMonthDay.prototype), "Temporal.PlainMonthDay.prototype");
