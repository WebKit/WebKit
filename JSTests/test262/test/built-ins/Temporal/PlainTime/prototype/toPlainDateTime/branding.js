// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.toplaindatetime
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const toPlainDateTime = Temporal.PlainTime.prototype.toPlainDateTime;

assert.sameValue(typeof toPlainDateTime, "function");

const args = [new Temporal.PlainDate(2022, 6, 22)];

assert.throws(TypeError, () => toPlainDateTime.apply(undefined, args), "undefined");
assert.throws(TypeError, () => toPlainDateTime.apply(null, args), "null");
assert.throws(TypeError, () => toPlainDateTime.apply(true, args), "true");
assert.throws(TypeError, () => toPlainDateTime.apply("", args), "empty string");
assert.throws(TypeError, () => toPlainDateTime.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => toPlainDateTime.apply(1, args), "1");
assert.throws(TypeError, () => toPlainDateTime.apply({}, args), "plain object");
assert.throws(TypeError, () => toPlainDateTime.apply(Temporal.PlainTime, args), "Temporal.PlainTime");
assert.throws(TypeError, () => toPlainDateTime.apply(Temporal.PlainTime.prototype, args), "Temporal.PlainTime.prototype");
