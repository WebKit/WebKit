// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthCode
description: >
  Temporal.Calendar.prototype.monthCode will take PlainYearMonth and return
  the value of the monthCode.
info: |
  6. Return ! ISOMonthCode(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let yearMonth = new Temporal.PlainYearMonth(1999, 6);
assert.sameValue(
  cal.monthCode(yearMonth),
  "M06",
  'cal.monthCode(new Temporal.PlainYearMonth(1999, 6)) must return "M06"'
);
