// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: Properties on objects passed to until() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalZonedDateTime
  "get other.calendar",
  "has other.calendar.dateAdd",
  "has other.calendar.dateFromFields",
  "has other.calendar.dateUntil",
  "has other.calendar.day",
  "has other.calendar.dayOfWeek",
  "has other.calendar.dayOfYear",
  "has other.calendar.daysInMonth",
  "has other.calendar.daysInWeek",
  "has other.calendar.daysInYear",
  "has other.calendar.fields",
  "has other.calendar.id",
  "has other.calendar.inLeapYear",
  "has other.calendar.mergeFields",
  "has other.calendar.month",
  "has other.calendar.monthCode",
  "has other.calendar.monthDayFromFields",
  "has other.calendar.monthsInYear",
  "has other.calendar.weekOfYear",
  "has other.calendar.year",
  "has other.calendar.yearMonthFromFields",
  "has other.calendar.yearOfWeek",
  "get other.calendar.dateFromFields",
  "get other.calendar.fields",
  "call other.calendar.fields",
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
  "has other.timeZone.getOffsetNanosecondsFor",
  "has other.timeZone.getPossibleInstantsFor",
  "has other.timeZone.id",
  "call other.calendar.dateFromFields",
  "get other.timeZone.getOffsetNanosecondsFor",
  "get other.timeZone.getPossibleInstantsFor",
  "call other.timeZone.getPossibleInstantsFor",
  "call other.timeZone.getOffsetNanosecondsFor",
  // CalendarEquals
  "get this.calendar.id",
  "get other.calendar.id",
  // CopyDataProperties
  "ownKeys options",
  "getOwnPropertyDescriptor options.roundingIncrement",
  "get options.roundingIncrement",
  "getOwnPropertyDescriptor options.roundingMode",
  "get options.roundingMode",
  "getOwnPropertyDescriptor options.largestUnit",
  "get options.largestUnit",
  "getOwnPropertyDescriptor options.smallestUnit",
  "get options.smallestUnit",
  "getOwnPropertyDescriptor options.additional",
  "get options.additional",
  // GetDifferenceSettings
  "get options.largestUnit.toString",
  "call options.largestUnit.toString",
  "get options.roundingIncrement.valueOf",
  "call options.roundingIncrement.valueOf",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
];
const actual = [];

const ownTimeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone");
const ownCalendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, ownTimeZone, ownCalendar);

const dstTimeZone = TemporalHelpers.springForwardFallBackTimeZone();
const ownDstTimeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone", {
  getOffsetNanosecondsFor: dstTimeZone.getOffsetNanosecondsFor,
  getPossibleInstantsFor: dstTimeZone.getPossibleInstantsFor,
});
const otherDstTimeZone = TemporalHelpers.timeZoneObserver(actual, "other.timeZone", {
  getOffsetNanosecondsFor: dstTimeZone.getOffsetNanosecondsFor,
  getPossibleInstantsFor: dstTimeZone.getPossibleInstantsFor,
});
/* 2000-10-29T01:30-07:00, in the middle of the first repeated hour: */
const fallBackInstance = new Temporal.ZonedDateTime(972808200_000_000_000n, ownDstTimeZone, ownCalendar);

const otherDateTimePropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2004,
  month: 5,
  monthCode: "M05",
  day: 12,
  hour: 1,
  minute: 46,
  second: 40,
  millisecond: 250,
  microsecond: 500,
  nanosecond: 750,
  offset: "+00:00",
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
  timeZone: TemporalHelpers.timeZoneObserver(actual, "other.timeZone"),
}, "other");

function createOptionsObserver({ smallestUnit = "nanoseconds", largestUnit = "auto", roundingMode = "halfExpand", roundingIncrement = 1 } = {}) {
  return TemporalHelpers.propertyBagObserver(actual, {
    // order is significant, due to iterating through properties in order to
    // copy them to an internal null-prototype object:
    roundingIncrement,
    roundingMode,
    largestUnit,
    smallestUnit,
    additional: "property",
  }, "options");
}

// clear any observable things that happened while constructing the objects
actual.splice(0);

// basic order of observable operations, without rounding:
instance.until(otherDateTimePropertyBag, createOptionsObserver());
assert.compareArray(actual, expected, "order of operations");
actual.splice(0); // clear

// short-circuit for identical objects will still test TimeZoneEquals if
// largestUnit is a calendar unit:
const identicalPropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 9,
  monthCode: "M09",
  day: 9,
  hour: 1,
  minute: 46,
  second: 40,
  millisecond: 0,
  microsecond: 0,
  nanosecond: 0,
  offset: "+00:00",
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
  timeZone: TemporalHelpers.timeZoneObserver(actual, "other.timeZone"),
}, "other");

instance.until(identicalPropertyBag, createOptionsObserver({ largestUnit: "years" }));
assert.compareArray(actual, expected.concat([
  "get this.timeZone.id",
  "get other.timeZone.id",
]), "order of operations with identical dates and largestUnit a calendar unit");
actual.splice(0); // clear

// two ZonedDateTimes that denote the same wall-clock time in the time zone can
// avoid calling some calendar methods:
const fallBackPropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 10,
  monthCode: "M10",
  day: 29,
  hour: 1,
  minute: 30,
  second: 0,
  millisecond: 0,
  microsecond: 0,
  nanosecond: 0,
  offset: "-08:00",
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
  timeZone: otherDstTimeZone,
}, "other");
fallBackInstance.until(fallBackPropertyBag, createOptionsObserver({ largestUnit: "days" }));
assert.compareArray(actual, [
  // ToTemporalZonedDateTime
  "get other.calendar",
  "has other.calendar.dateAdd",
  "has other.calendar.dateFromFields",
  "has other.calendar.dateUntil",
  "has other.calendar.day",
  "has other.calendar.dayOfWeek",
  "has other.calendar.dayOfYear",
  "has other.calendar.daysInMonth",
  "has other.calendar.daysInWeek",
  "has other.calendar.daysInYear",
  "has other.calendar.fields",
  "has other.calendar.id",
  "has other.calendar.inLeapYear",
  "has other.calendar.mergeFields",
  "has other.calendar.month",
  "has other.calendar.monthCode",
  "has other.calendar.monthDayFromFields",
  "has other.calendar.monthsInYear",
  "has other.calendar.weekOfYear",
  "has other.calendar.year",
  "has other.calendar.yearMonthFromFields",
  "has other.calendar.yearOfWeek",
  "get other.calendar.dateFromFields",
  "get other.calendar.fields",
  "call other.calendar.fields",
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
  "has other.timeZone.getOffsetNanosecondsFor",
  "has other.timeZone.getPossibleInstantsFor",
  "has other.timeZone.id",
  "call other.calendar.dateFromFields",
  "get other.timeZone.getOffsetNanosecondsFor",
  "get other.timeZone.getPossibleInstantsFor",
  "call other.timeZone.getPossibleInstantsFor",
  "call other.timeZone.getOffsetNanosecondsFor",
  // NOTE: extra because of wall-clock time ambiguity:
  "call other.timeZone.getOffsetNanosecondsFor",
  // CalendarEquals
  "get this.calendar.id",
  "get other.calendar.id",
  // CopyDataProperties
  "ownKeys options",
  "getOwnPropertyDescriptor options.roundingIncrement",
  "get options.roundingIncrement",
  "getOwnPropertyDescriptor options.roundingMode",
  "get options.roundingMode",
  "getOwnPropertyDescriptor options.largestUnit",
  "get options.largestUnit",
  "getOwnPropertyDescriptor options.smallestUnit",
  "get options.smallestUnit",
  "getOwnPropertyDescriptor options.additional",
  "get options.additional",
  // GetDifferenceSettings
  "get options.largestUnit.toString",
  "call options.largestUnit.toString",
  "get options.roundingIncrement.valueOf",
  "call options.roundingIncrement.valueOf",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
  // TimeZoneEquals
  "get this.timeZone.id",
  "get other.timeZone.id",
  // lookup
  "get this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getPossibleInstantsFor",
  "get this.calendar.dateAdd",
  "get this.calendar.dateUntil",
  // DifferenceZonedDateTime
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getPossibleInstantsFor",
], "order of operations with identical wall-clock times and largestUnit a calendar unit");
actual.splice(0); // clear

// Making largestUnit a calendar unit adds the following observable operations:
const expectedOpsForCalendarDifference = [
  // TimeZoneEquals
  "get this.timeZone.id",
  "get other.timeZone.id",
  // lookup
  "get this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getPossibleInstantsFor",
  "get this.calendar.dateAdd",
  "get this.calendar.dateUntil",
  // precalculate PlainDateTime
  "call this.timeZone.getOffsetNanosecondsFor",
  // DifferenceZonedDateTime
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getPossibleInstantsFor",
  "call this.calendar.dateUntil",
];

const expectedOpsForCalendarRounding = [
  // RoundDuration → MoveRelativeZonedDateTime → AddZonedDateTime
  "call this.calendar.dateAdd",
  "call this.timeZone.getPossibleInstantsFor",
  // RoundDuration → NanosecondsToDays
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // RoundDuration → NanosecondsToDays → AddDaysToZonedDateTime
  "call this.timeZone.getPossibleInstantsFor",
];

// code path that skips RoundDuration:
instance.until(otherDateTimePropertyBag, createOptionsObserver({ largestUnit: "years", smallestUnit: "nanoseconds", roundingIncrement: 1 }));
assert.compareArray(actual, expected.concat(expectedOpsForCalendarDifference), "order of operations with largestUnit years and no rounding");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expected.concat(expectedOpsForCalendarDifference, expectedOpsForCalendarRounding, [
  // RoundDuration
  "call this.calendar.dateAdd",    // 12.d
  "call this.calendar.dateAdd",    // 12.f
  "call this.calendar.dateUntil",  // 12.n
  "call this.calendar.dateAdd",    // 12.x MoveRelativeDate
  // (12.r not called because other units can't add up to >1 year at this point)
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 9.c
  "call this.calendar.dateUntil"   // 9.d
]);
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "years" }));
assert.compareArray(actual, expectedOpsForYearRounding, "order of operations with smallestUnit = years");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest month:
const expectedOpsForMonthRounding = expected.concat(expectedOpsForCalendarDifference, expectedOpsForCalendarRounding, [
  // RoundDuration
  "call this.calendar.dateAdd",    // 13.c
  "call this.calendar.dateAdd",    // 13.e
  "call this.calendar.dateUntil",  // 13.m
  "call this.calendar.dateAdd",    // 13.w MoveRelativeDate
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 10.d
  "call this.calendar.dateUntil",  // 10.e
]);
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "months" }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with smallestUnit = months");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest week:
const expectedOpsForWeekRounding = expected.concat(expectedOpsForCalendarDifference, expectedOpsForCalendarRounding, [
  // RoundDuration
  "call this.calendar.dateUntil",  // 14.f
  "call this.calendar.dateAdd",    // 14.p MoveRelativeDate
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 16
  "call this.calendar.dateUntil",  // 17
]);
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "weeks" }));
assert.compareArray(actual, expectedOpsForWeekRounding, "order of operations with smallestUnit = weeks");
actual.splice(0);  // clear

instance.until(otherDateTimePropertyBag, createOptionsObserver({ largestUnit: "hours" }));
assert.compareArray(actual, expected, "order of operations with largestUnit being a time unit");
actual.splice(0);  // clear
