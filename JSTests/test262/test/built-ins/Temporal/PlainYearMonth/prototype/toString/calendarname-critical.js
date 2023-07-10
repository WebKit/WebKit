// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.tostring
description: >
  If calendarName is "calendar", the calendar ID should be included and prefixed
  with "!".
features: [Temporal]
---*/

const calendarMethods = {
  dateAdd() {},
  dateFromFields() {},
  dateUntil() {},
  day() {},
  dayOfWeek() {},
  dayOfYear() {},
  daysInMonth() {},
  daysInWeek() {},
  daysInYear() {},
  fields() {},
  inLeapYear() {},
  mergeFields() {},
  month() {},
  monthCode() {},
  monthDayFromFields() {},
  monthsInYear() {},
  weekOfYear() {},
  year() {},
  yearMonthFromFields() {},
  yearOfWeek() {},
};

const tests = [
  [[], "2000-05-01[!u-ca=iso8601]", "built-in ISO"],
  [[{ id: "custom", ...calendarMethods }], "2000-05-01[!u-ca=custom]", "custom"],
  [[{ id: "iso8601", ...calendarMethods }], "2000-05-01[!u-ca=iso8601]", "custom with iso8601 id"],
  [[{ id: "ISO8601", ...calendarMethods }], "2000-05-01[!u-ca=ISO8601]", "custom with caps id"],
  [[{ id: "\u0131so8601", ...calendarMethods }], "2000-05-01[!u-ca=\u0131so8601]", "custom with dotless i id"],
];

for (const [args, expected, description] of tests) {
  const yearmonth = new Temporal.PlainYearMonth(2000, 5, ...args);
  const result = yearmonth.toString({ calendarName: "critical" });
  assert.sameValue(result, expected, `${description} calendar for calendarName = critical`);
}
