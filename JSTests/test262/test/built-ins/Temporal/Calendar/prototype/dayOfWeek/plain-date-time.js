// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.dayofweek
description: >
  Temporal.Calendar.prototype.dayOfWeek will take Temporal.PlainDateTime objects
  and return the day of week.
info: |
  5. Return 𝔽(! ToISODayOfWeek(temporalDate.[[ISOYear]], temporalDate.[[ISOMonth]], temporalDate.[[ISODay]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let dt = new Temporal.PlainDateTime(1997, 1, 23, 5, 30, 13);
assert.sameValue(
  cal.dayOfWeek(dt),
  4,
  'cal.dayOfWeek(new Temporal.PlainDateTime(1997, 1, 23, 5, 30, 13)) must return 4'
);
dt = new Temporal.PlainDateTime(1996, 2, 23, 5, 30, 13);
assert.sameValue(
  cal.dayOfWeek(dt),
  5,
  'cal.dayOfWeek(new Temporal.PlainDateTime(1996, 2, 23, 5, 30, 13)) must return 5'
);
dt = new Temporal.PlainDateTime(1997, 2, 23, 5, 30, 13);
assert.sameValue(
  cal.dayOfWeek(dt),
  7,
  'cal.dayOfWeek(new Temporal.PlainDateTime(1997, 2, 23, 5, 30, 13)) must return 7'
);
dt = new Temporal.PlainDateTime(1997, 6, 23, 5, 30, 13);
assert.sameValue(
  cal.dayOfWeek(dt),
  1,
  'cal.dayOfWeek(new Temporal.PlainDateTime(1997, 6, 23, 5, 30, 13)) must return 1'
);
