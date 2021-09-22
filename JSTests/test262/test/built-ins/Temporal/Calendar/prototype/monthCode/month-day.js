// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthCode
description: >
  Temporal.Calendar.prototype.monthCode will take PlainMonthDay and return
  the value of the monthCode.
info: |
  6. Return ! ISOMonthCode(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let monthDay = new Temporal.PlainMonthDay(12, 25);
assert.sameValue(
  cal.monthCode(monthDay),
  "M12",
  'cal.monthCode(new Temporal.PlainMonthDay(12, 25)) must return "M12"'
);
