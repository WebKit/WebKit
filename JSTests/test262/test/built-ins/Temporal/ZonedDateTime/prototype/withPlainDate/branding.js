// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const withPlainDate = Temporal.ZonedDateTime.prototype.withPlainDate;

assert.sameValue(typeof withPlainDate, "function");

assert.throws(TypeError, () => withPlainDate.call(undefined), "undefined");
assert.throws(TypeError, () => withPlainDate.call(null), "null");
assert.throws(TypeError, () => withPlainDate.call(true), "true");
assert.throws(TypeError, () => withPlainDate.call(""), "empty string");
assert.throws(TypeError, () => withPlainDate.call(Symbol()), "symbol");
assert.throws(TypeError, () => withPlainDate.call(1), "1");
assert.throws(TypeError, () => withPlainDate.call({}), "plain object");
assert.throws(TypeError, () => withPlainDate.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => withPlainDate.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
