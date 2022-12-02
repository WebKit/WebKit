// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: Properties on an object passed to add() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalDurationRecord
  "get fields.days",
  "get fields.days.valueOf",
  "call fields.days.valueOf",
  "get fields.hours",
  "get fields.hours.valueOf",
  "call fields.hours.valueOf",
  "get fields.microseconds",
  "get fields.microseconds.valueOf",
  "call fields.microseconds.valueOf",
  "get fields.milliseconds",
  "get fields.milliseconds.valueOf",
  "call fields.milliseconds.valueOf",
  "get fields.minutes",
  "get fields.minutes.valueOf",
  "call fields.minutes.valueOf",
  "get fields.months",
  "get fields.months.valueOf",
  "call fields.months.valueOf",
  "get fields.nanoseconds",
  "get fields.nanoseconds.valueOf",
  "call fields.nanoseconds.valueOf",
  "get fields.seconds",
  "get fields.seconds.valueOf",
  "call fields.seconds.valueOf",
  "get fields.weeks",
  "get fields.weeks.valueOf",
  "call fields.weeks.valueOf",
  "get fields.years",
  "get fields.years.valueOf",
  "call fields.years.valueOf",
  // ToRelativeTemporalObject
  "get options.relativeTo",
];
const actual = [];

const simpleFields = TemporalHelpers.propertyBagObserver(actual, {
  years: 0,
  months: 0,
  weeks: 0,
  days: 1,
  hours: 1,
  minutes: 1,
  seconds: 1,
  milliseconds: 1,
  microseconds: 1,
  nanoseconds: 1,
}, "fields");

function createOptionsObserver(relativeTo = undefined) {
  return TemporalHelpers.propertyBagObserver(actual, { relativeTo }, "options");
}

// basic order of observable operations, without any calendar units:
const simpleInstance = new Temporal.Duration(0, 0, 0, 1, 1, 1, 1, 1, 1, 1);
simpleInstance.add(simpleFields, createOptionsObserver());
assert.compareArray(actual, expected, "order of operations");
actual.splice(0); // clear

const expectedOpsForPlainRelativeTo = expected.concat([
  // ToRelativeTemporalObject
  "get options.relativeTo.calendar",
  "has options.relativeTo.calendar.calendar",
  "get options.relativeTo.calendar.fields",
  "call options.relativeTo.calendar.fields",
  // PrepareTemporalFields
  "get options.relativeTo.day",
  "get options.relativeTo.day.valueOf",
  "call options.relativeTo.day.valueOf",
  "get options.relativeTo.hour",
  "get options.relativeTo.microsecond",
  "get options.relativeTo.millisecond",
  "get options.relativeTo.minute",
  "get options.relativeTo.month",
  "get options.relativeTo.month.valueOf",
  "call options.relativeTo.month.valueOf",
  "get options.relativeTo.monthCode",
  "get options.relativeTo.monthCode.toString",
  "call options.relativeTo.monthCode.toString",
  "get options.relativeTo.nanosecond",
  "get options.relativeTo.offset",
  "get options.relativeTo.second",
  "get options.relativeTo.timeZone",
  "get options.relativeTo.year",
  "get options.relativeTo.year.valueOf",
  "call options.relativeTo.year.valueOf",
  // InterpretTemporalDateTimeFields
  "get options.relativeTo.calendar.dateFromFields",
  "call options.relativeTo.calendar.dateFromFields",
  // AddDuration
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.calendar.dateUntil",
  "call options.relativeTo.calendar.dateUntil",
]);

const instance = new Temporal.Duration(1, 2, 0, 4, 5, 6, 7, 987, 654, 321);

const fields = TemporalHelpers.propertyBagObserver(actual, {
  years: 1,
  months: 1,
  weeks: 1,
  days: 1,
  hours: 1,
  minutes: 1,
  seconds: 1,
  milliseconds: 1,
  microseconds: 1,
  nanoseconds: 1,
}, "fields");

const plainRelativeTo = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 1,
  monthCode: "M01",
  day: 1,
  calendar: TemporalHelpers.calendarObserver(actual, "options.relativeTo.calendar"),
}, "options.relativeTo");

instance.add(fields, createOptionsObserver(plainRelativeTo));
assert.compareArray(actual, expectedOpsForPlainRelativeTo, "order of operations with PlainDate relativeTo");
actual.splice(0); // clear

const expectedOpsForZonedRelativeTo = expected.concat([
  // ToRelativeTemporalObject
  "get options.relativeTo.calendar",
  "has options.relativeTo.calendar.calendar",
  "get options.relativeTo.calendar.fields",
  "call options.relativeTo.calendar.fields",
  // PrepareTemporalFields
  "get options.relativeTo.day",
  "get options.relativeTo.day.valueOf",
  "call options.relativeTo.day.valueOf",
  "get options.relativeTo.hour",
  "get options.relativeTo.hour.valueOf",
  "call options.relativeTo.hour.valueOf",
  "get options.relativeTo.microsecond",
  "get options.relativeTo.microsecond.valueOf",
  "call options.relativeTo.microsecond.valueOf",
  "get options.relativeTo.millisecond",
  "get options.relativeTo.millisecond.valueOf",
  "call options.relativeTo.millisecond.valueOf",
  "get options.relativeTo.minute",
  "get options.relativeTo.minute.valueOf",
  "call options.relativeTo.minute.valueOf",
  "get options.relativeTo.month",
  "get options.relativeTo.month.valueOf",
  "call options.relativeTo.month.valueOf",
  "get options.relativeTo.monthCode",
  "get options.relativeTo.monthCode.toString",
  "call options.relativeTo.monthCode.toString",
  "get options.relativeTo.nanosecond",
  "get options.relativeTo.nanosecond.valueOf",
  "call options.relativeTo.nanosecond.valueOf",
  "get options.relativeTo.offset",
  "get options.relativeTo.offset.toString",
  "call options.relativeTo.offset.toString",
  "get options.relativeTo.second",
  "get options.relativeTo.second.valueOf",
  "call options.relativeTo.second.valueOf",
  "get options.relativeTo.timeZone",
  "get options.relativeTo.year",
  "get options.relativeTo.year.valueOf",
  "call options.relativeTo.year.valueOf",
  // InterpretTemporalDateTimeFields
  "get options.relativeTo.calendar.dateFromFields",
  "call options.relativeTo.calendar.dateFromFields",
  // ToRelativeTemporalObject again
  "has options.relativeTo.timeZone.timeZone",
  // InterpretISODateTimeOffset
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // AddDuration → AddZonedDateTime 1
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // AddDuration → AddZonedDateTime 2
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // AddDuration → DifferenceZonedDateTime
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // AddDuration → DifferenceZonedDateTime → DifferenceISODateTime
  "get options.relativeTo.calendar.dateUntil",
  "call options.relativeTo.calendar.dateUntil",
  // AddDuration → DifferenceZonedDateTime → AddZonedDateTime
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // AddDuration → DifferenceZonedDateTime → NanosecondsToDays
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // AddDuration → DifferenceZonedDateTime → NanosecondsToDays → DifferenceISODateTime
  "get options.relativeTo.calendar.dateUntil",
  "call options.relativeTo.calendar.dateUntil",
  // AddDuration → DifferenceZonedDateTime → NanosecondsToDays → AddZonedDateTime 1
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // AddDuration → DifferenceZonedDateTime → NanosecondsToDays → AddZonedDateTime 2
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
]);

const zonedRelativeTo = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 1,
  monthCode: "M01",
  day: 1,
  hour: 0,
  minute: 0,
  second: 0,
  millisecond: 0,
  microsecond: 0,
  nanosecond: 0,
  offset: "+00:00",
  calendar: TemporalHelpers.calendarObserver(actual, "options.relativeTo.calendar"),
  timeZone: TemporalHelpers.timeZoneObserver(actual, "options.relativeTo.timeZone"),
}, "options.relativeTo");

instance.add(fields, createOptionsObserver(zonedRelativeTo));
assert.compareArray(actual, expectedOpsForZonedRelativeTo, "order of operations with ZonedDateTime relativeTo");
