// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withtimezone
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const withTimeZone = Temporal.ZonedDateTime.prototype.withTimeZone;

assert.sameValue(typeof withTimeZone, "function");

assert.throws(TypeError, () => withTimeZone.call(undefined), "undefined");
assert.throws(TypeError, () => withTimeZone.call(null), "null");
assert.throws(TypeError, () => withTimeZone.call(true), "true");
assert.throws(TypeError, () => withTimeZone.call(""), "empty string");
assert.throws(TypeError, () => withTimeZone.call(Symbol()), "symbol");
assert.throws(TypeError, () => withTimeZone.call(1), "1");
assert.throws(TypeError, () => withTimeZone.call({}), "plain object");
assert.throws(TypeError, () => withTimeZone.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => withTimeZone.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
