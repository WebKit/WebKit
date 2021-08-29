// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const since = Temporal.PlainDate.prototype.since;

assert.sameValue(typeof since, "function");

assert.throws(TypeError, () => since.call(undefined), "undefined");
assert.throws(TypeError, () => since.call(null), "null");
assert.throws(TypeError, () => since.call(true), "true");
assert.throws(TypeError, () => since.call(""), "empty string");
assert.throws(TypeError, () => since.call(Symbol()), "symbol");
assert.throws(TypeError, () => since.call(1), "1");
assert.throws(TypeError, () => since.call({}), "plain object");
assert.throws(TypeError, () => since.call(Temporal.PlainDate), "Temporal.PlainDate");
assert.throws(TypeError, () => since.call(Temporal.PlainDate.prototype), "Temporal.PlainDate.prototype");
