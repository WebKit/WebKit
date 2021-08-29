// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toZonedDateTime = Temporal.PlainDate.prototype.toZonedDateTime;

assert.sameValue(typeof toZonedDateTime, "function");

assert.throws(TypeError, () => toZonedDateTime.call(undefined), "undefined");
assert.throws(TypeError, () => toZonedDateTime.call(null), "null");
assert.throws(TypeError, () => toZonedDateTime.call(true), "true");
assert.throws(TypeError, () => toZonedDateTime.call(""), "empty string");
assert.throws(TypeError, () => toZonedDateTime.call(Symbol()), "symbol");
assert.throws(TypeError, () => toZonedDateTime.call(1), "1");
assert.throws(TypeError, () => toZonedDateTime.call({}), "plain object");
assert.throws(TypeError, () => toZonedDateTime.call(Temporal.PlainDate), "Temporal.PlainDate");
assert.throws(TypeError, () => toZonedDateTime.call(Temporal.PlainDate.prototype), "Temporal.PlainDate.prototype");
