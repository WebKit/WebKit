// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.month
description: >
  Temporal.Calendar.prototype.month throws TypeError if temporalDateLike
  is a PlainMonthDay object.
  ToTemporalDate when temporalDateLike is invalid string.
info: |
  4. If Type(temporalDateLike) is Object and temporalDateLike has an
    [[InitializedTemporalMonthDay]] internal slot, then
    a. Throw a TypeError exception.
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

let monthDay = new Temporal.PlainMonthDay(12, 25);
assert.throws(TypeError, () => cal.month(monthDay),
    'cal.month(monthDay) throws a TypeError exception');
