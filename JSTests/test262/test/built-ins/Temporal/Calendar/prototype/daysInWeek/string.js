// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysinweeks
description: >
  Temporal.Calendar.prototype.daysInWeek will take valid ISO8601 string
  and return 7.
info: |
  4. Perform ? ToTemporalDate(temporalDateLike).
  5. Return 7𝔽.
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");
assert.sameValue(cal.daysInWeek("2019-03-18"), 7, 'cal.daysInWeek("2019-03-18") must return 7');
