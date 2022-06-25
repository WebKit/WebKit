// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaintime.prototype.minute
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const minute = Object.getOwnPropertyDescriptor(Temporal.PlainTime.prototype, "minute").get;

assert.sameValue(typeof minute, "function");

assert.throws(TypeError, () => minute.call(undefined), "undefined");
assert.throws(TypeError, () => minute.call(null), "null");
assert.throws(TypeError, () => minute.call(true), "true");
assert.throws(TypeError, () => minute.call(""), "empty string");
assert.throws(TypeError, () => minute.call(Symbol()), "symbol");
assert.throws(TypeError, () => minute.call(1), "1");
assert.throws(TypeError, () => minute.call({}), "plain object");
assert.throws(TypeError, () => minute.call(Temporal.PlainTime), "Temporal.PlainTime");
assert.throws(TypeError, () => minute.call(Temporal.PlainTime.prototype), "Temporal.PlainTime.prototype");
