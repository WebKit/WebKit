// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const withPlainDate = Temporal.PlainDateTime.prototype.withPlainDate;

assert.sameValue(typeof withPlainDate, "function");

assert.throws(TypeError, () => withPlainDate.call(undefined), "undefined");
assert.throws(TypeError, () => withPlainDate.call(null), "null");
assert.throws(TypeError, () => withPlainDate.call(true), "true");
assert.throws(TypeError, () => withPlainDate.call(""), "empty string");
assert.throws(TypeError, () => withPlainDate.call(Symbol()), "symbol");
assert.throws(TypeError, () => withPlainDate.call(1), "1");
assert.throws(TypeError, () => withPlainDate.call({}), "plain object");
assert.throws(TypeError, () => withPlainDate.call(Temporal.PlainDateTime), "Temporal.PlainDateTime");
assert.throws(TypeError, () => withPlainDate.call(Temporal.PlainDateTime.prototype), "Temporal.PlainDateTime.prototype");
