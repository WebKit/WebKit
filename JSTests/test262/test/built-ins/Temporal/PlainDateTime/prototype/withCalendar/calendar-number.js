// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withcalendar
description: A number is converted to a string, then to Temporal.Calendar
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, {
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
  id: "replace-me",
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
});

const arg = 19761118;

const result = instance.withCalendar(arg);
assert.sameValue(result.calendarId, "iso8601", "19761118 is a valid ISO string for Calendar");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.withCalendar(arg),
    `Number ${arg} does not convert to a valid ISO string for Calendar`
  );
}
