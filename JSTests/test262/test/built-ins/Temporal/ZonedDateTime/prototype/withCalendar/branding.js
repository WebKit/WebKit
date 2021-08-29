// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withcalendar
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const withCalendar = Temporal.ZonedDateTime.prototype.withCalendar;

assert.sameValue(typeof withCalendar, "function");

assert.throws(TypeError, () => withCalendar.call(undefined), "undefined");
assert.throws(TypeError, () => withCalendar.call(null), "null");
assert.throws(TypeError, () => withCalendar.call(true), "true");
assert.throws(TypeError, () => withCalendar.call(""), "empty string");
assert.throws(TypeError, () => withCalendar.call(Symbol()), "symbol");
assert.throws(TypeError, () => withCalendar.call(1), "1");
assert.throws(TypeError, () => withCalendar.call({}), "plain object");
assert.throws(TypeError, () => withCalendar.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => withCalendar.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
