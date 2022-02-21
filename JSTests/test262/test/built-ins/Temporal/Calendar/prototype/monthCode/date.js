// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthCode
description: >
  Temporal.Calendar.prototype.monthCode will take PlainDate and return
  the value of the monthCode.
info: |
  5. Return ! ISOMonthCode(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let date = new Temporal.PlainDate(2021, 7, 15);
assert.sameValue(cal.monthCode(date), "M07", 'cal.monthCode(new Temporal.PlainDate(2021, 7, 15)) must return "M07"');
