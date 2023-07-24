// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withcalendar
description: Fast path for converting other Temporal objects to Temporal.Calendar by reading internal slots
info: |
    sec-temporal-totemporalcalendar step 1.b:
      b. If _temporalCalendarLike_ has an [[InitializedTemporalDate]], [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]], [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]] internal slot, then
        i. Return _temporalCalendarLike_.[[Calendar]].
includes: [compareArray.js]
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(2000, 5, 2);
const plainDateTime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321);
const plainMonthDay = new Temporal.PlainMonthDay(5, 2);
const plainYearMonth = new Temporal.PlainYearMonth(2000, 5);
const zonedDateTime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC");

[plainDate, plainDateTime, plainMonthDay, plainYearMonth, zonedDateTime].forEach((arg) => {
  const actual = [];
  const expected = [];

  const calendar = arg.getISOFields().calendar;

  Object.defineProperty(arg, "calendar", {
    get() {
      actual.push("get calendar");
      return calendar;
    },
  });

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
  const result = instance.withCalendar(arg);
  assert.sameValue(result.getISOFields().calendar, calendar, "Temporal object coerced to calendar");

  assert.compareArray(actual, expected, "calendar getter not called");
});
