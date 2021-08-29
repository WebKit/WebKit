// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getOffsetStringFor = Temporal.TimeZone.prototype.getOffsetStringFor;

assert.sameValue(typeof getOffsetStringFor, "function");

assert.throws(TypeError, () => getOffsetStringFor.call(undefined), "undefined");
assert.throws(TypeError, () => getOffsetStringFor.call(null), "null");
assert.throws(TypeError, () => getOffsetStringFor.call(true), "true");
assert.throws(TypeError, () => getOffsetStringFor.call(""), "empty string");
assert.throws(TypeError, () => getOffsetStringFor.call(Symbol()), "symbol");
assert.throws(TypeError, () => getOffsetStringFor.call(1), "1");
assert.throws(TypeError, () => getOffsetStringFor.call({}), "plain object");
assert.throws(TypeError, () => getOffsetStringFor.call(Temporal.TimeZone), "Temporal.TimeZone");
assert.throws(TypeError, () => getOffsetStringFor.call(Temporal.TimeZone.prototype), "Temporal.TimeZone.prototype");
