// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.getcalendar
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const getCalendar = Temporal.PlainDate.prototype.getCalendar;

assert.sameValue(typeof getCalendar, "function");

assert.throws(TypeError, () => getCalendar.call(undefined), "undefined");
assert.throws(TypeError, () => getCalendar.call(null), "null");
assert.throws(TypeError, () => getCalendar.call(true), "true");
assert.throws(TypeError, () => getCalendar.call(""), "empty string");
assert.throws(TypeError, () => getCalendar.call(Symbol()), "symbol");
assert.throws(TypeError, () => getCalendar.call(1), "1");
assert.throws(TypeError, () => getCalendar.call({}), "plain object");
assert.throws(TypeError, () => getCalendar.call(Temporal.PlainDate), "Temporal.PlainDate");
assert.throws(TypeError, () => getCalendar.call(Temporal.PlainDate.prototype), "Temporal.PlainDate.prototype");
