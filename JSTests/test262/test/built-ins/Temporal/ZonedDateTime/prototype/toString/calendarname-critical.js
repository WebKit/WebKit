// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.tostring
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
  [[], "1970-01-01T01:01:01.987654321+00:00[UTC][!u-ca=iso8601]", "built-in ISO"],
  [[{ id: "custom", ...calendarMethods }], "1970-01-01T01:01:01.987654321+00:00[UTC][!u-ca=custom]", "custom"],
  [[{ id: "iso8601", ...calendarMethods }], "1970-01-01T01:01:01.987654321+00:00[UTC][!u-ca=iso8601]", "custom with iso8601 id"],
  [[{ id: "ISO8601", ...calendarMethods }], "1970-01-01T01:01:01.987654321+00:00[UTC][!u-ca=ISO8601]", "custom with caps id"],
  [[{ id: "\u0131so8601", ...calendarMethods }], "1970-01-01T01:01:01.987654321+00:00[UTC][!u-ca=\u0131so8601]", "custom with dotless i id"],
];

for (const [args, expected, description] of tests) {
  const date = new Temporal.ZonedDateTime(3661_987_654_321n, "UTC", ...args);
  const result = date.toString({ calendarName: "critical" });
  assert.sameValue(result, expected, `${description} calendar for calendarName = critical`);
}
