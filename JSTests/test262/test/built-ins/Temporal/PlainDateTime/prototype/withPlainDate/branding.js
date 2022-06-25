// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const withPlainDate = Temporal.PlainDateTime.prototype.withPlainDate;

assert.sameValue(typeof withPlainDate, "function");

const args = [new Temporal.PlainDate(2022, 6, 22)];

assert.throws(TypeError, () => withPlainDate.apply(undefined, args), "undefined");
assert.throws(TypeError, () => withPlainDate.apply(null, args), "null");
assert.throws(TypeError, () => withPlainDate.apply(true, args), "true");
assert.throws(TypeError, () => withPlainDate.apply("", args), "empty string");
assert.throws(TypeError, () => withPlainDate.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => withPlainDate.apply(1, args), "1");
assert.throws(TypeError, () => withPlainDate.apply({}, args), "plain object");
assert.throws(TypeError, () => withPlainDate.apply(Temporal.PlainDateTime, args), "Temporal.PlainDateTime");
assert.throws(TypeError, () => withPlainDate.apply(Temporal.PlainDateTime.prototype, args), "Temporal.PlainDateTime.prototype");
