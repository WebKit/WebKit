// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysinweeks
description: Temporal.Calendar.prototype.daysInWeek will take PlainDateTime and return 7.
info: |
  4. Perform ? ToTemporalDate(temporalDateLike).
  5. Return 7𝔽.
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let dt = new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInWeek(dt),
  7,
  'cal.daysInWeek(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)) must return 7'
);
