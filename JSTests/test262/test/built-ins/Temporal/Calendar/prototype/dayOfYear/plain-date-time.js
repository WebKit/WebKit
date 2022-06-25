// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.dayofyear
description: >
  Temporal.Calendar.prototype.dayOfYear will take PlainDateTime object and
  return the day of year.
info: |
  4. Let temporalDate be ? ToTemporalDate(temporalDateLike).
  5. Return 𝔽(! ToISODayOfYear(temporalDate.[[ISOYear]], temporalDate.[[ISOMonth]], temporalDate.[[ISODay]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let dt = new Temporal.PlainDateTime(1997, 1, 23, 5, 30, 13);
assert.sameValue(
  cal.dayOfYear(dt),
  23,
  'cal.dayOfYear(new Temporal.PlainDateTime(1997, 1, 23, 5, 30, 13)) must return 23'
);

dt = new Temporal.PlainDateTime(1997, 2, 23, 5, 30, 13);
assert.sameValue(
  cal.dayOfYear(dt),
  54,
  'cal.dayOfYear(new Temporal.PlainDateTime(1997, 2, 23, 5, 30, 13)) must return 54'
);

dt = new Temporal.PlainDateTime(1996, 3, 23, 5, 30, 13);
assert.sameValue(
  cal.dayOfYear(dt),
  83,
  'cal.dayOfYear(new Temporal.PlainDateTime(1996, 3, 23, 5, 30, 13)) must return 83'
);

dt = new Temporal.PlainDateTime(1997, 3, 23, 5, 30, 13);
assert.sameValue(
  cal.dayOfYear(dt),
  82,
  'cal.dayOfYear(new Temporal.PlainDateTime(1997, 3, 23, 5, 30, 13)) must return 82'
);

dt = new Temporal.PlainDateTime(1997, 12, 31, 5, 30, 13);
assert.sameValue(
  cal.dayOfYear(dt),
  365,
  'cal.dayOfYear(new Temporal.PlainDateTime(1997, 12, 31, 5, 30, 13)) must return 365'
);

dt = new Temporal.PlainDateTime(1996, 12, 31, 5, 30, 13);
assert.sameValue(
  cal.dayOfYear(dt),
  366,
  'cal.dayOfYear(new Temporal.PlainDateTime(1996, 12, 31, 5, 30, 13)) must return 366'
);
