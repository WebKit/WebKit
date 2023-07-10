// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: PlainDate calendar is preserved with ISO PDT
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const cal = {
  id: 'thisisnotiso',
  era() { return undefined; },
  eraYear() { return undefined; },
  toString() { return "this is a string"; },
  year() { return 2008; },
  month() { return 9; },
  monthCode() { return "M09"; },
  day() { return 6; },
  dateAdd() {},
  dateFromFields() {},
  dateUntil() {},
  dayOfWeek() {},
  dayOfYear() {},
  daysInMonth() {},
  daysInWeek() {},
  daysInYear() {},
  fields() {},
  inLeapYear() {},
  mergeFields() {},
  monthDayFromFields() {},
  monthsInYear() {},
  weekOfYear() {},
  yearMonthFromFields() {},
  yearOfWeek() {},
};
const pdt = new Temporal.PlainDateTime(1995, 12, 7, 3, 24, 30, 0, 0, 0);
assert.sameValue(pdt.calendarId, "iso8601", "PlainDateTime with ISO calendar");
const pd = new Temporal.PlainDate(2010, 11, 12, cal);
const shifted = pdt.withPlainDate(pd);

TemporalHelpers.assertPlainDateTime(
  shifted,
  2008, 9, "M09", 6, 3, 24, 30, 0, 0, 0,
  "calendar is changed if receiver has ISO calendar (1)"
  // Testing of era and eraYear should only be coded under intl402
);

assert.sameValue(
  shifted.getCalendar(),
  cal,
  "calendar is changed if receiver has ISO calendar (2)"
);
