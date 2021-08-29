// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.dayOfYear
description: Temporal.Calendar.prototype.dayOfYear throws TypeError when the internal lot is not presented.
info: |
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

let badCal = { dayOfYear: cal.dayOfYear }
assert.throws(TypeError, () => badCal.dayOfYear("2021-03-04"),
    'badCal.dayOfYear("2021-03-04") throws a TypeError exception');
