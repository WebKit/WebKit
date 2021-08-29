// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetnanosecondsfor
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getOffsetNanosecondsFor = Temporal.TimeZone.prototype.getOffsetNanosecondsFor;

assert.sameValue(typeof getOffsetNanosecondsFor, "function");

assert.throws(TypeError, () => getOffsetNanosecondsFor.call(undefined), "undefined");
assert.throws(TypeError, () => getOffsetNanosecondsFor.call(null), "null");
assert.throws(TypeError, () => getOffsetNanosecondsFor.call(true), "true");
assert.throws(TypeError, () => getOffsetNanosecondsFor.call(""), "empty string");
assert.throws(TypeError, () => getOffsetNanosecondsFor.call(Symbol()), "symbol");
assert.throws(TypeError, () => getOffsetNanosecondsFor.call(1), "1");
assert.throws(TypeError, () => getOffsetNanosecondsFor.call({}), "plain object");
assert.throws(TypeError, () => getOffsetNanosecondsFor.call(Temporal.TimeZone), "Temporal.TimeZone");
assert.throws(TypeError, () => getOffsetNanosecondsFor.call(Temporal.TimeZone.prototype), "Temporal.TimeZone.prototype");
