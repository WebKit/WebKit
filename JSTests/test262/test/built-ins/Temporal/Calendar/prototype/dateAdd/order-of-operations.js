// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Properties on an object passed to dateAdd() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalDate → GetTemporalCalendarWithISODefault
  "get date.calendar",
  "has date.calendar.calendar",
  // ToTemporalDate → CalendarFields
  "get date.calendar.fields",
  "call date.calendar.fields",
  // ToTemporalDate → PrepareTemporalFields
  "get date.day",
  "get date.day.valueOf",
  "call date.day.valueOf",
  "get date.month",
  "get date.month.valueOf",
  "call date.month.valueOf",
  "get date.monthCode",
  "get date.monthCode.toString",
  "call date.monthCode.toString",
  "get date.year",
  "get date.year.valueOf",
  "call date.year.valueOf",
  // ToTemporalDate → CalendarDateFromFields
  "get date.calendar.dateFromFields",
  "call date.calendar.dateFromFields",
  // ToTemporalDuration
  "get duration.days",
  "get duration.days.valueOf",
  "call duration.days.valueOf",
  "get duration.hours",
  "get duration.hours.valueOf",
  "call duration.hours.valueOf",
  "get duration.microseconds",
  "get duration.microseconds.valueOf",
  "call duration.microseconds.valueOf",
  "get duration.milliseconds",
  "get duration.milliseconds.valueOf",
  "call duration.milliseconds.valueOf",
  "get duration.minutes",
  "get duration.minutes.valueOf",
  "call duration.minutes.valueOf",
  "get duration.months",
  "get duration.months.valueOf",
  "call duration.months.valueOf",
  "get duration.nanoseconds",
  "get duration.nanoseconds.valueOf",
  "call duration.nanoseconds.valueOf",
  "get duration.seconds",
  "get duration.seconds.valueOf",
  "call duration.seconds.valueOf",
  "get duration.weeks",
  "get duration.weeks.valueOf",
  "call duration.weeks.valueOf",
  "get duration.years",
  "get duration.years.valueOf",
  "call duration.years.valueOf",
  // ToTemporalOverflow
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
];
const actual = [];

const instance = new Temporal.Calendar("iso8601");

const date = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 5,
  monthCode: "M05",
  day: 2,
  calendar: TemporalHelpers.calendarObserver(actual, "date.calendar"),
}, "date");

const duration = TemporalHelpers.propertyBagObserver(actual, {
  years: 1,
  months: 2,
  weeks: 3,
  days: 4,
  hours: 5,
  minutes: 6,
  seconds: 7,
  milliseconds: 8,
  microseconds: 9,
  nanoseconds: 10,
}, "duration");

const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

instance.dateAdd(date, duration, options);
assert.compareArray(actual, expected, "order of operations");
