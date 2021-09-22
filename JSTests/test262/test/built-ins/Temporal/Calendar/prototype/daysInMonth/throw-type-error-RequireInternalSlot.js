// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysInMonth
description: >
  Temporal.Calendar.prototype.daysInMonth throws TypeError
  when the internal lot is not presented.
info: |
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

let badCal = { daysInMonth: cal.daysInMonth }
assert.throws(TypeError, () => badCal.daysInMonth("2021-03-04"),
    'badCal.daysInMonth("2021-03-04") throws a TypeError exception');
