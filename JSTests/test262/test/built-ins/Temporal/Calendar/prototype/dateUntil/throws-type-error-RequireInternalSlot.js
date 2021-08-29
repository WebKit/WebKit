// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Temporal.Calendar.prototype.dateUntil throw TypeError on RequireInternalSlot
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");
let badCal = { dateUntil: cal.dateUntil };

assert.throws(TypeError, () => badCal.dateUntil("2021-07-16", "2021-07-17"),
      'badCal.dateUntil("2021-07-16", "2021-07-17") throws a TypeError exception');
