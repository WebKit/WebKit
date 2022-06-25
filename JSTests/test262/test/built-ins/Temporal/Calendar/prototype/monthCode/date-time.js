// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthCode
description: >
  Temporal.Calendar.prototype.month will take PlainDateTime and return
  the value of the monthCode.
info: |
  6. Return ! ISOMonthCode(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let dateTime = new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)
assert.sameValue(
  cal.monthCode(dateTime),
  "M08",
  'cal.monthCode(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)) must return "M08"'
);
