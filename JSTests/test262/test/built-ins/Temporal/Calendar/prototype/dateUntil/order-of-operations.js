// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Properties on an object passed to dateUntil() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalDate 1 → GetTemporalCalendarSlotValueWithISODefault
  "get one.calendar",
  "has one.calendar.dateAdd",
  "has one.calendar.dateFromFields",
  "has one.calendar.dateUntil",
  "has one.calendar.day",
  "has one.calendar.dayOfWeek",
  "has one.calendar.dayOfYear",
  "has one.calendar.daysInMonth",
  "has one.calendar.daysInWeek",
  "has one.calendar.daysInYear",
  "has one.calendar.fields",
  "has one.calendar.id",
  "has one.calendar.inLeapYear",
  "has one.calendar.mergeFields",
  "has one.calendar.month",
  "has one.calendar.monthCode",
  "has one.calendar.monthDayFromFields",
  "has one.calendar.monthsInYear",
  "has one.calendar.weekOfYear",
  "has one.calendar.year",
  "has one.calendar.yearMonthFromFields",
  "has one.calendar.yearOfWeek",
  // ToTemporalDate 1 → CalendarFields
  "get one.calendar.fields",
  "call one.calendar.fields",
  // ToTemporalDate 1 → PrepareTemporalFields
  "get one.day",
  "get one.day.valueOf",
  "call one.day.valueOf",
  "get one.month",
  "get one.month.valueOf",
  "call one.month.valueOf",
  "get one.monthCode",
  "get one.monthCode.toString",
  "call one.monthCode.toString",
  "get one.year",
  "get one.year.valueOf",
  "call one.year.valueOf",
  // ToTemporalDate 1 → CalendarDateFromFields
  "get one.calendar.dateFromFields",
  "call one.calendar.dateFromFields",
  // ToTemporalDate 2 → GetTemporalCalendarSlotValueWithISODefault
  "get two.calendar",
  "has two.calendar.dateAdd",
  "has two.calendar.dateFromFields",
  "has two.calendar.dateUntil",
  "has two.calendar.day",
  "has two.calendar.dayOfWeek",
  "has two.calendar.dayOfYear",
  "has two.calendar.daysInMonth",
  "has two.calendar.daysInWeek",
  "has two.calendar.daysInYear",
  "has two.calendar.fields",
  "has two.calendar.id",
  "has two.calendar.inLeapYear",
  "has two.calendar.mergeFields",
  "has two.calendar.month",
  "has two.calendar.monthCode",
  "has two.calendar.monthDayFromFields",
  "has two.calendar.monthsInYear",
  "has two.calendar.weekOfYear",
  "has two.calendar.year",
  "has two.calendar.yearMonthFromFields",
  "has two.calendar.yearOfWeek",
  // ToTemporalDate 2 → CalendarFields
  "get two.calendar.fields",
  "call two.calendar.fields",
  // ToTemporalDate 2 → PrepareTemporalFields
  "get two.day",
  "get two.day.valueOf",
  "call two.day.valueOf",
  "get two.month",
  "get two.month.valueOf",
  "call two.month.valueOf",
  "get two.monthCode",
  "get two.monthCode.toString",
  "call two.monthCode.toString",
  "get two.year",
  "get two.year.valueOf",
  "call two.year.valueOf",
  // ToTemporalDate 2 → CalendarDateFromFields
  "get two.calendar.dateFromFields",
  "call two.calendar.dateFromFields",
  // GetTemporalUnit
  "get options.largestUnit",
  "get options.largestUnit.toString",
  "call options.largestUnit.toString",
];
const actual = [];

const instance = new Temporal.Calendar("iso8601");

const one = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 5,
  monthCode: "M05",
  day: 2,
  calendar: TemporalHelpers.calendarObserver(actual, "one.calendar"),
}, "one");

const two = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 10,
  monthCode: "M10",
  day: 4,
  calendar: TemporalHelpers.calendarObserver(actual, "two.calendar"),
}, "two");

const options = TemporalHelpers.propertyBagObserver(actual, { largestUnit: "day" }, "options");

instance.dateUntil(one, two, options);
assert.compareArray(actual, expected, "order of operations");
