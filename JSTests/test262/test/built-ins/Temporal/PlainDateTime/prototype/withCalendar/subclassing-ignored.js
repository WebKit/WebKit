// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withcalendar
description: Objects of a subclass are never created as return values.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const customCalendar = {
  era() { return undefined; },
  eraYear() { return undefined; },
  year() { return 1900; },
  month() { return 2; },
  monthCode() { return "M02"; },
  day() { return 5; },
  id: "custom-calendar",
  toString() { return "custom-calendar"; },
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

TemporalHelpers.checkSubclassingIgnored(
  Temporal.PlainDateTime,
  [2000, 5, 2, 12, 34, 56, 987, 654, 321],
  "withCalendar",
  [customCalendar],
  (result) => {
    TemporalHelpers.assertPlainDateTime(result, 1900, 2, "M02", 5, 12, 34, 56, 987, 654, 321);
    assert.sameValue(result.getCalendar(), customCalendar, "calendar result");
  },
);
