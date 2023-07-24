// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.gettimezone
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getTimeZone = Temporal.ZonedDateTime.prototype.getTimeZone;

assert.sameValue(typeof getTimeZone, "function");

assert.throws(TypeError, () => getTimeZone.call(undefined), "undefined");
assert.throws(TypeError, () => getTimeZone.call(null), "null");
assert.throws(TypeError, () => getTimeZone.call(true), "true");
assert.throws(TypeError, () => getTimeZone.call(""), "empty string");
assert.throws(TypeError, () => getTimeZone.call(Symbol()), "symbol");
assert.throws(TypeError, () => getTimeZone.call(1), "1");
assert.throws(TypeError, () => getTimeZone.call({}), "plain object");
assert.throws(TypeError, () => getTimeZone.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => getTimeZone.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
