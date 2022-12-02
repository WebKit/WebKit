// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: Properties on an object passed to from() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // GetTemporalCalendarWithISODefault
  "get fields.calendar",
  "has fields.calendar.calendar",
  // CalendarFields
  "get fields.calendar.fields",
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
  "get fields.calendar.yearMonthFromFields",
  "call fields.calendar.yearMonthFromFields",
  // inside Calendar.p.yearMonthFromFields
  "get options.overflow",
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

const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

Temporal.PlainYearMonth.from(fields, options);
assert.compareArray(actual, expected, "order of operations");
