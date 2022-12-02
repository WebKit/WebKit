// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: Properties on objects passed to equals() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get other.calendar",
  "has other.calendar.calendar",
  "get other.calendar.fields",
  "call other.calendar.fields",
  // PrepareTemporalFields
  "get other.day",
  "get other.day.valueOf",
  "call other.day.valueOf",
  "get other.hour",
  "get other.hour.valueOf",
  "call other.hour.valueOf",
  "get other.microsecond",
  "get other.microsecond.valueOf",
  "call other.microsecond.valueOf",
  "get other.millisecond",
  "get other.millisecond.valueOf",
  "call other.millisecond.valueOf",
  "get other.minute",
  "get other.minute.valueOf",
  "call other.minute.valueOf",
  "get other.month",
  "get other.month.valueOf",
  "call other.month.valueOf",
  "get other.monthCode",
  "get other.monthCode.toString",
  "call other.monthCode.toString",
  "get other.nanosecond",
  "get other.nanosecond.valueOf",
  "call other.nanosecond.valueOf",
  "get other.offset",
  "get other.offset.toString",
  "call other.offset.toString",
  "get other.second",
  "get other.second.valueOf",
  "call other.second.valueOf",
  "get other.timeZone",
  "get other.year",
  "get other.year.valueOf",
  "call other.year.valueOf",
  "has other.timeZone.timeZone",
  // InterpretTemporalDateTimeFields
  "get other.calendar.dateFromFields",
  "call other.calendar.dateFromFields",
  // InterpretISODateTimeOffset
  "get other.timeZone.getPossibleInstantsFor",
  "call other.timeZone.getPossibleInstantsFor",
  "get other.timeZone.getOffsetNanosecondsFor",
  "call other.timeZone.getOffsetNanosecondsFor",
  // TimeZoneEquals
  "get this.timeZone[Symbol.toPrimitive]",
  "get this.timeZone.toString",
  "call this.timeZone.toString",
  "get other.timeZone[Symbol.toPrimitive]",
  "get other.timeZone.toString",
  "call other.timeZone.toString",
  // CalendarEquals
  "get this.calendar[Symbol.toPrimitive]",
  "get this.calendar.toString",
  "call this.calendar.toString",
  "get other.calendar[Symbol.toPrimitive]",
  "get other.calendar.toString",
  "call other.calendar.toString",
];
const actual = [];

const other = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 5,
  monthCode: "M05",
  day: 2,
  hour: 6,
  minute: 54,
  second: 32,
  millisecond: 987,
  microsecond: 654,
  nanosecond: 321,
  offset: "+00:00",
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
  timeZone: TemporalHelpers.timeZoneObserver(actual, "other.timeZone"),
}, "other");

const instance = new Temporal.ZonedDateTime(
  988786472_987_654_321n,  /* 2001-05-02T06:54:32.987654321Z */
  TemporalHelpers.timeZoneObserver(actual, "this.timeZone"),
  TemporalHelpers.calendarObserver(actual, "this.calendar"),
);
// clear any observable operations that happen due to time zone or calendar
// calls on the constructor
actual.splice(0);

instance.equals(other);
assert.compareArray(actual, expected, "order of operations");
