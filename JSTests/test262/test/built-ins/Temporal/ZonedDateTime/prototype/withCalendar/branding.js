// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withcalendar
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const withCalendar = Temporal.ZonedDateTime.prototype.withCalendar;

assert.sameValue(typeof withCalendar, "function");

const args = [new Temporal.Calendar("iso8601")];

assert.throws(TypeError, () => withCalendar.apply(undefined, args), "undefined");
assert.throws(TypeError, () => withCalendar.apply(null, args), "null");
assert.throws(TypeError, () => withCalendar.apply(true, args), "true");
assert.throws(TypeError, () => withCalendar.apply("", args), "empty string");
assert.throws(TypeError, () => withCalendar.apply(Symbol(), args), "symbol");
assert.throws(TypeError, () => withCalendar.apply(1, args), "1");
assert.throws(TypeError, () => withCalendar.apply({}, args), "plain object");
assert.throws(TypeError, () => withCalendar.apply(Temporal.ZonedDateTime, args), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => withCalendar.apply(Temporal.ZonedDateTime.prototype, args), "Temporal.ZonedDateTime.prototype");
