// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.with
description: Properties on objects passed to with() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // RejectObjectWithCalendarOrTimeZone
  "get fields.calendar",
  "get fields.timeZone",
  // CalendarFields / PrepareTemporalFields
  "get this.calendar.fields",
  "call this.calendar.fields",
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
  "get fields.offset",
  "get fields.offset.toString",
  "call fields.offset.toString",
  "get fields.second",
  "get fields.second.valueOf",
  "call fields.second.valueOf",
  "get fields.year",
  "get fields.year.valueOf",
  "call fields.year.valueOf",
  // PrepareTemporalFields
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.day",
  "call this.calendar.day",
  "get this.timeZone.getOffsetNanosecondsFor",  // ZonedDateTime.p.hour
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",  // ZonedDateTime.p.microsecond
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",  // ZonedDateTime.p.millisecond
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",  // ZonedDateTime.p.minute
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.month",
  "call this.calendar.month",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.timeZone.getOffsetNanosecondsFor",  // ZonedDateTime.p.nanosecond
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",  // ZonedDateTime.p.offset
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",  // ZonedDateTime.p.second
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.year",
  "call this.calendar.year",
  // CalendarMergeFields
  "get this.calendar.mergeFields",
  "call this.calendar.mergeFields",
  // InterpretTemporalDateTimeFields
  "get options.disambiguation",
  "get options.disambiguation.toString",
  "call options.disambiguation.toString",
  "get options.offset",
  "get options.offset.toString",
  "call options.offset.toString",
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  "get this.calendar.dateFromFields",
  "call this.calendar.dateFromFields",
  // Calendar.p.dateFromFields
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  // InterpretISODateTimeOffset
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
];
const actual = [];

const timeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone");
const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.ZonedDateTime(0n, timeZone, calendar);
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
  offset: "+00:00",
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, {
  overflow: "constrain",
  disambiguation: "compatible",
  offset: "prefer",
}, "options");

instance.with(fields, options);
assert.compareArray(actual, expected, "order of operations");
