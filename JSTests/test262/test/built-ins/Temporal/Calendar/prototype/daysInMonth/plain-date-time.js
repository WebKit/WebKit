// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysinmonth
description: >
  Temporal.Calendar.prototype.daysInMonth will take Temporal.PlainDateTime object
  and return the number of days in that month.
info: |
  4. If Type(temporalDateLike) is not Object or temporalDateLike does not have an [[InitializedTemporalDate]] or [[InitializedTemporalYearMonth]] internal slots, then
    a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
  5. Return 𝔽(! ISODaysInMonth(temporalDateLike.[[ISOYear]], temporalDateLike.[[ISOMonth]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let dt = new Temporal.PlainDateTime(1997, 1, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  31,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 1, 23, 5, 30, 13)) must return 31'
);

// leap year
dt = new Temporal.PlainDateTime(1996, 2, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  29,
  'cal.daysInMonth(new Temporal.PlainDateTime(1996, 2, 23, 5, 30, 13)) must return 29'
);
dt = new Temporal.PlainDateTime(2000, 2, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  29,
  'cal.daysInMonth(new Temporal.PlainDateTime(2000, 2, 23, 5, 30, 13)) must return 29'
);

// non leap year
dt = new Temporal.PlainDateTime(1997, 2, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  28,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 2, 23, 5, 30, 13)) must return 28'
);

dt = new Temporal.PlainDateTime(1997, 3, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  31,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 3, 23, 5, 30, 13)) must return 31'
);

dt = new Temporal.PlainDateTime(1997, 4, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  30,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 4, 23, 5, 30, 13)) must return 30'
);

dt = new Temporal.PlainDateTime(1997, 5, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  31,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 5, 23, 5, 30, 13)) must return 31'
);

dt = new Temporal.PlainDateTime(1997, 6, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  30,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 6, 23, 5, 30, 13)) must return 30'
);

dt = new Temporal.PlainDateTime(1997, 7, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  31,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 7, 23, 5, 30, 13)) must return 31'
);

dt = new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  31,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 8, 23, 5, 30, 13)) must return 31'
);

dt = new Temporal.PlainDateTime(1997, 9, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  30,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 9, 23, 5, 30, 13)) must return 30'
);

dt = new Temporal.PlainDateTime(1997, 10, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  31,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 10, 23, 5, 30, 13)) must return 31'
);

dt = new Temporal.PlainDateTime(1997, 11, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  30,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 11, 23, 5, 30, 13)) must return 30'
);

dt = new Temporal.PlainDateTime(1997, 12, 23, 5, 30, 13);
assert.sameValue(
  cal.daysInMonth(dt),
  31,
  'cal.daysInMonth(new Temporal.PlainDateTime(1997, 12, 23, 5, 30, 13)) must return 31'
);
