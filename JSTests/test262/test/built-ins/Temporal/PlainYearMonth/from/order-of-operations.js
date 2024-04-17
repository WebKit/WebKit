// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: Properties on an object passed to from() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // CopyDataProperties
  "ownKeys options",
  "getOwnPropertyDescriptor options.overflow",
  "get options.overflow",
  "getOwnPropertyDescriptor options.extra",
  "get options.extra",
  // GetTemporalCalendarSlotValueWithISODefault
  "get fields.calendar",
  "has fields.calendar.dateAdd",
  "has fields.calendar.dateFromFields",
  "has fields.calendar.dateUntil",
  "has fields.calendar.day",
  "has fields.calendar.dayOfWeek",
  "has fields.calendar.dayOfYear",
  "has fields.calendar.daysInMonth",
  "has fields.calendar.daysInWeek",
  "has fields.calendar.daysInYear",
  "has fields.calendar.fields",
  "has fields.calendar.id",
  "has fields.calendar.inLeapYear",
  "has fields.calendar.mergeFields",
  "has fields.calendar.month",
  "has fields.calendar.monthCode",
  "has fields.calendar.monthDayFromFields",
  "has fields.calendar.monthsInYear",
  "has fields.calendar.weekOfYear",
  "has fields.calendar.year",
  "has fields.calendar.yearMonthFromFields",
  "has fields.calendar.yearOfWeek",
  // lookup
  "get fields.calendar.fields",
  "get fields.calendar.yearMonthFromFields",
  // CalendarFields
  "call fields.calendar.fields",
  // PrepareTemporalFields
  "get fields.month",
  "get fields.month.valueOf",
  "call fields.month.valueOf",
  "get fields.monthCode",
  "get fields.monthCode.toString",
  "call fields.monthCode.toString",
  "get fields.year",
  "get fields.year.valueOf",
  "call fields.year.valueOf",
  // CalendarYearMonthFromFields
  "call fields.calendar.yearMonthFromFields",
  // inside Calendar.p.yearMonthFromFields
  "get options.overflow.toString",
  "call options.overflow.toString",
];
const actual = [];

const fields = TemporalHelpers.propertyBagObserver(actual, {
  year: 1.7,
  month: 1.7,
  monthCode: "M01",
  calendar: TemporalHelpers.calendarObserver(actual, "fields.calendar"),
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, {
  overflow: "constrain",
  extra: "property",
}, "options");

Temporal.PlainYearMonth.from(fields, options);
assert.compareArray(actual, expected, "order of operations");
