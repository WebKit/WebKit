// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.with
description: Properties on an object passed to with() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // RejectObjectWithCalendarOrTimeZone
  "get fields.calendar",
  "get fields.timeZone",
  // CalendarFields
  "get this.calendar.fields",
  "call this.calendar.fields",
  // PrepareTemporalFields on receiver
  "get this.calendar.month",
  "call this.calendar.month",
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  // PrepareTemporalFields on argument
  "get fields.month",
  "get fields.month.valueOf",
  "call fields.month.valueOf",
  "get fields.monthCode",
  "get fields.monthCode.toString",
  "call fields.monthCode.toString",
  "get fields.year",
  "get fields.year.valueOf",
  "call fields.year.valueOf",
  // CalendarMergeFields
  "get this.calendar.mergeFields",
  "call this.calendar.mergeFields",
  // CalendarYearMonthFromFields
  "get this.calendar.yearMonthFromFields",
  "call this.calendar.yearMonthFromFields",
  // inside Calendar.p.yearMonthFromFields
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
];
const actual = [];

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainYearMonth(2000, 5, calendar);
// clear observable operations that occurred during the constructor call
actual.splice(0);

const fields = TemporalHelpers.propertyBagObserver(actual, {
  year: 1.7,
  month: 1.7,
  monthCode: "M01",
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

instance.with(fields, options);
assert.compareArray(actual, expected, "order of operations");
