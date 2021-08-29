// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd should throw if calendar does not have required internal slot
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

let badCal = { dateAdd: cal.dateAdd };
assert.throws(TypeError,
    () => badCal.dateAdd("2020-02-29", new Temporal.Duration(1)),
    'badCal.dateAdd("2020-02-29", new Temporal.Duration(1)) throws a TypeError exception');
