// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: Properties on an object passed to subtract() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalDuration
  "get fields.days",
  "get fields.days.valueOf",
  "call fields.days.valueOf",
  "get fields.hours",
  "get fields.hours.valueOf",
  "call fields.hours.valueOf",
  "get fields.microseconds",
  "get fields.microseconds.valueOf",
  "call fields.microseconds.valueOf",
  "get fields.milliseconds",
  "get fields.milliseconds.valueOf",
  "call fields.milliseconds.valueOf",
  "get fields.minutes",
  "get fields.minutes.valueOf",
  "call fields.minutes.valueOf",
  "get fields.months",
  "get fields.months.valueOf",
  "call fields.months.valueOf",
  "get fields.nanoseconds",
  "get fields.nanoseconds.valueOf",
  "call fields.nanoseconds.valueOf",
  "get fields.seconds",
  "get fields.seconds.valueOf",
  "call fields.seconds.valueOf",
  "get fields.weeks",
  "get fields.weeks.valueOf",
  "call fields.weeks.valueOf",
  "get fields.years",
  "get fields.years.valueOf",
  "call fields.years.valueOf",
  // CalendarFields
  "get this.calendar.fields",
  "call this.calendar.fields",
  // PrepareTemporalFields on receiver
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  // CalendarDaysInMonth
  "get this.calendar.dateFromFields",
  "call this.calendar.dateFromFields",
  "get this.calendar.dateAdd",
  "call this.calendar.dateAdd",
  "get this.calendar.day",
  "call this.calendar.day",
  "get this.calendar.dateFromFields",
  "call this.calendar.dateFromFields",
  // CopyDataProperties
  "ownKeys options",
  "getOwnPropertyDescriptor options.overflow",
  "get options.overflow",
  // CalendarDateAdd
  "call this.calendar.dateAdd",
  // inside Calendar.p.dateAdd
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  // PrepareTemporalFields on added date
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  // CalendarYearMonthFromFields
  "get this.calendar.yearMonthFromFields",
  "call this.calendar.yearMonthFromFields",
  // inside Calendar.p.yearMonthFromFields
  "get options.overflow.toString",
  "call options.overflow.toString",
];
const actual = [];

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainYearMonth(2000, 5, calendar);
// clear observable operations that occurred during the constructor call
actual.splice(0);

const fields = TemporalHelpers.propertyBagObserver(actual, {
  years: 1,
  months: 1,
  weeks: 1,
  days: 1,
  hours: 1,
  minutes: 1,
  seconds: 1,
  milliseconds: 1,
  microseconds: 1,
  nanoseconds: 1,
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

instance.subtract(fields, options);
assert.compareArray(actual, expected, "order of operations");

actual.splice(0); // clear

const noCalendarExpected = [
  // ToTemporalDuration
  "get fields.days",
  "get fields.days.valueOf",
  "call fields.days.valueOf",
  "get fields.hours",
  "get fields.hours.valueOf",
  "call fields.hours.valueOf",
  "get fields.microseconds",
  "get fields.microseconds.valueOf",
  "call fields.microseconds.valueOf",
  "get fields.milliseconds",
  "get fields.milliseconds.valueOf",
  "call fields.milliseconds.valueOf",
  "get fields.minutes",
  "get fields.minutes.valueOf",
  "call fields.minutes.valueOf",
  "get fields.months",
  "get fields.nanoseconds",
  "get fields.nanoseconds.valueOf",
  "call fields.nanoseconds.valueOf",
  "get fields.seconds",
  "get fields.seconds.valueOf",
  "call fields.seconds.valueOf",
  "get fields.weeks",
  "get fields.years",
  // CalendarFields
  "get this.calendar.fields",
  "call this.calendar.fields",
  // PrepareTemporalFields on receiver
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  // CalendarDateFromFields
  "get this.calendar.dateFromFields",
  "call this.calendar.dateFromFields",
  "get this.calendar.dateAdd",
  "call this.calendar.dateAdd",
  "get this.calendar.day",
  "call this.calendar.day",
  "get this.calendar.dateFromFields",
  "call this.calendar.dateFromFields",
  // SnapshotOwnProperties
  "ownKeys options",
  "getOwnPropertyDescriptor options.overflow",
  "get options.overflow",
  // AddDate
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  // PrepareTemporalFields on added date
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  // CalendarYearMonthFromFields
  "get this.calendar.yearMonthFromFields",
  "call this.calendar.yearMonthFromFields",
  // inside Calendar.p.yearMonthFromFields
  "get options.overflow.toString",
  "call options.overflow.toString",
];

const noCalendarFields = TemporalHelpers.propertyBagObserver(actual, {
  days: 1,
  hours: 1,
  minutes: 1,
  seconds: 1,
  milliseconds: 1,
  microseconds: 1,
  nanoseconds: 1,
}, "fields");

instance.subtract(noCalendarFields, options);
assert.compareArray(actual, noCalendarExpected, "order of operations with no calendar units");

actual.splice(0); // clear
