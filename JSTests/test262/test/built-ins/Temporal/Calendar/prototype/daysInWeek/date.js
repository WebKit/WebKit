// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysinweeks
description: Temporal.Calendar.prototype.daysInWeek will take PlainDate and return 7.
info: |
  5. Return 7𝔽.
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let d = new Temporal.PlainDate(2021, 7, 15);
assert.sameValue(cal.daysInWeek(d), 7, 'cal.daysInWeek(new Temporal.PlainDate(2021, 7, 15)) must return 7');
