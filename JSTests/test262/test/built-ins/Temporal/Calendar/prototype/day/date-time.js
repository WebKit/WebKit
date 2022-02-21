// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.day
description: >
  Temporal.Calendar.prototype.day will take PlainDateTime and return
  the value of the day.
info: |
  5. Return ! ISODay(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let dateTime = new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)
assert.sameValue(cal.day(dateTime), 23, 'cal.day(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)) must return 23');
