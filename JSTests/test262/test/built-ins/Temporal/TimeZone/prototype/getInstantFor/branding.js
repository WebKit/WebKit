// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getInstantFor = Temporal.TimeZone.prototype.getInstantFor;

assert.sameValue(typeof getInstantFor, "function");

assert.throws(TypeError, () => getInstantFor.call(undefined), "undefined");
assert.throws(TypeError, () => getInstantFor.call(null), "null");
assert.throws(TypeError, () => getInstantFor.call(true), "true");
assert.throws(TypeError, () => getInstantFor.call(""), "empty string");
assert.throws(TypeError, () => getInstantFor.call(Symbol()), "symbol");
assert.throws(TypeError, () => getInstantFor.call(1), "1");
assert.throws(TypeError, () => getInstantFor.call({}), "plain object");
assert.throws(TypeError, () => getInstantFor.call(Temporal.TimeZone), "Temporal.TimeZone");
assert.throws(TypeError, () => getInstantFor.call(Temporal.TimeZone.prototype), "Temporal.TimeZone.prototype");
