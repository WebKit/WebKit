// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.from
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
  "get fields.calendar.dateFromFields",
  "get fields.calendar.fields",
  // CalendarFields
  "call fields.calendar.fields",
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
  // InterpretTemporalDateTimeFields
  "get options.overflow.toString",
  "call options.overflow.toString",
  "call fields.calendar.dateFromFields",
];
const actual = [];

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
  calendar: TemporalHelpers.calendarObserver(actual, "fields.calendar"),
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, {
  overflow: "constrain",
  extra: "property",
}, "options");

Temporal.PlainDateTime.from(fields, options);
assert.compareArray(actual, expected, "order of operations");
