// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: Properties on an object passed to from() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get fields.calendar",
  // ToTemporalCalendarWithISODefault
  "has fields.calendar.calendar",
  // CalendarFields
  "get fields.calendar.fields",
  "call fields.calendar.fields",
  // PrepareTemporalFields
  "get fields.day",
  "get fields.day.valueOf",
  "call fields.day.valueOf",
  "get fields.month",
  "get fields.month.valueOf",
  "call fields.month.valueOf",
  "get fields.monthCode",
  "get fields.monthCode.toString",
  "call fields.monthCode.toString",
  "get fields.year",
  "get fields.year.valueOf",
  "call fields.year.valueOf",
  // CalendarMonthDayFromFields
  "get fields.calendar.monthDayFromFields",
  "call fields.calendar.monthDayFromFields",
  // inside Calendar.p.monthDayFromFields
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
];
const actual = [];

const fields = TemporalHelpers.propertyBagObserver(actual, {
  year: 1.7,
  month: 1.7,
  monthCode: "M01",
  day: 1.7,
  calendar: TemporalHelpers.calendarObserver(actual, "fields.calendar"),
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

Temporal.PlainMonthDay.from(fields, options);
assert.compareArray(actual, expected, "order of operations");
