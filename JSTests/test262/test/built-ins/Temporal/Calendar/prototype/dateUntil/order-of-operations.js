// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Properties on an object passed to dateUntil() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalDate 1 → GetTemporalCalendarWithISODefault
  "get one.calendar",
  "has one.calendar.calendar",
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
  // ToTemporalDate 2 → GetTemporalCalendarWithISODefault
  "get two.calendar",
  "has two.calendar.calendar",
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
