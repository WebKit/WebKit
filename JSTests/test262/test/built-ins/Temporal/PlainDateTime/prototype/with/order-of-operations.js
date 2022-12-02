// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.with
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
  // PrepareTemporalFields
  "get fields.day",
  "get fields.day.valueOf",
  "call fields.day.valueOf",
  "get fields.hour",
  "get fields.hour.valueOf",
  "call fields.hour.valueOf",
  "get fields.microsecond",
  "get fields.microsecond.valueOf",
  "call fields.microsecond.valueOf",
  "get fields.millisecond",
  "get fields.millisecond.valueOf",
  "call fields.millisecond.valueOf",
  "get fields.minute",
  "get fields.minute.valueOf",
  "call fields.minute.valueOf",
  "get fields.month",
  "get fields.month.valueOf",
  "call fields.month.valueOf",
  "get fields.monthCode",
  "get fields.monthCode.toString",
  "call fields.monthCode.toString",
  "get fields.nanosecond",
  "get fields.nanosecond.valueOf",
  "call fields.nanosecond.valueOf",
  "get fields.second",
  "get fields.second.valueOf",
  "call fields.second.valueOf",
  "get fields.year",
  "get fields.year.valueOf",
  "call fields.year.valueOf",
  // PrepareTemporalFields on receiver
  "get this.calendar.day",
  "call this.calendar.day",
  "get this.calendar.month",
  "call this.calendar.month",
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  // CalendarMergeFields
  "get this.calendar.mergeFields",
  "call this.calendar.mergeFields",
  // InterpretTemporalDateTimeFields
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  "get this.calendar.dateFromFields",
  "call this.calendar.dateFromFields",
  // inside Calendar.p.dateFromFields
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
];
const actual = [];

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
// clear observable operations that occurred during the constructor call
actual.splice(0);

const fields = TemporalHelpers.propertyBagObserver(actual, {
  year: 1.7,
  month: 1.7,
  monthCode: "M01",
  day: 1.7,
  hour: 1.7,
  minute: 1.7,
  second: 1.7,
  millisecond: 1.7,
  microsecond: 1.7,
  nanosecond: 1.7,
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

instance.with(fields, options);
assert.compareArray(actual, expected, "order of operations");
