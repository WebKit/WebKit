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
  "has other.calendar.calendar",
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
  "get other.second",
  "get other.second.valueOf",
  "call other.second.valueOf",
  "get other.year",
  "get other.year.valueOf",
  "call other.year.valueOf",
  "get other.timeZone",
  "get other.offset",
  "has other.timeZone.timeZone",
  "get other.calendar.dateFromFields",
  "call other.calendar.dateFromFields",
  "get other.timeZone.getPossibleInstantsFor",
  "call other.timeZone.getPossibleInstantsFor",
  // CalendarEquals
  "get this.calendar[Symbol.toPrimitive]",
  "get this.calendar.toString",
  "call this.calendar.toString",
  "get other.calendar[Symbol.toPrimitive]",
  "get other.calendar.toString",
  "call other.calendar.toString",
  // GetDifferenceSettings
  "get options.smallestUnit",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
  "get options.largestUnit",
  "get options.largestUnit.toString",
  "call options.largestUnit.toString",
  "get options.roundingMode",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.roundingIncrement",
  "get options.roundingIncrement.valueOf",
  "call options.roundingIncrement.valueOf",
];
const actual = [];

const ownTimeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone");
const ownCalendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, ownTimeZone, ownCalendar);

const otherDateTimePropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 5,
  monthCode: "M05",
  day: 2,
  hour: 1,
  minute: 46,
  second: 40,
  millisecond: 250,
  microsecond: 500,
  nanosecond: 750,
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
actual.splice(0, actual.length);

// basic order of observable operations, without rounding:
instance.until(otherDateTimePropertyBag, createOptionsObserver());
assert.compareArray(actual, expected, "order of operations");
actual.splice(0, actual.length); // clear

// Making largestUnit a calendar unit adds the following observable operations:
const expectedOpsForCalendarDifference = [
  // TimeZoneEquals
  "get this.timeZone[Symbol.toPrimitive]",
  "get this.timeZone.toString",
  "call this.timeZone.toString",
  "get other.timeZone[Symbol.toPrimitive]",
  "get other.timeZone.toString",
  "call other.timeZone.toString",
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
  // DifferenceZonedDateTime
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // DifferenceISODateTime
  "get this.calendar.dateUntil",
  "call this.calendar.dateUntil",
  // AddZonedDateTime
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.dateAdd",
  "call this.calendar.dateAdd",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // NanosecondsToDays
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // NanosecondsToDays → DifferenceISODateTime
  "get this.calendar.dateUntil",
  "call this.calendar.dateUntil",
  // NanosecondsToDays → AddZonedDateTime
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.dateAdd",
  "call this.calendar.dateAdd",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // BalanceDuration → AddZonedDateTime
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.dateAdd",
  "call this.calendar.dateAdd",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // RoundDuration → ToTemporalDate
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // RoundDuration → MoveRelativeZonedDateTime → AddZonedDateTime
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.dateAdd",
  "call this.calendar.dateAdd",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
  // RoundDuration → NanosecondsToDays
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // RoundDuration → NanosecondsToDays → DifferenceISODateTime
  "get this.calendar.dateUntil",
  "call this.calendar.dateUntil",
  // RoundDuration → NanosecondsToDays → AddZonedDateTime
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.calendar.dateAdd",
  "call this.calendar.dateAdd",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
];

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expected.concat(expectedOpsForCalendarDifference, [
  "get this.calendar.dateAdd",     // 9.b
  "call this.calendar.dateAdd",    // 9.c
  "call this.calendar.dateAdd",    // 9.e
  "call this.calendar.dateAdd",    // 9.j
  "get this.calendar.dateUntil",   // 9.m
  "call this.calendar.dateUntil",  // 9.m
  "call this.calendar.dateAdd",    // 9.r
  "call this.calendar.dateAdd",    // 9.w MoveRelativeDate
]);
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "years" }));
assert.compareArray(actual, expectedOpsForYearRounding, "order of operations with smallestUnit = years");
actual.splice(0, actual.length); // clear

// code path through RoundDuration that rounds to the nearest month:
const expectedOpsForMonthRounding = expected.concat(expectedOpsForCalendarDifference, [
  "get this.calendar.dateAdd",     // 10.b
  "call this.calendar.dateAdd",    // 10.c
  "call this.calendar.dateAdd",    // 10.e
  "call this.calendar.dateAdd",    // 10.k MoveRelativeDate
]);  // (10.n.iii MoveRelativeDate not called because weeks == 0)
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "months" }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with smallestUnit = months");
actual.splice(0, actual.length); // clear

// code path through RoundDuration that rounds to the nearest week:
const expectedOpsForWeekRounding = expected.concat(expectedOpsForCalendarDifference, [
  "get this.calendar.dateAdd",   // 11.c
  "call this.calendar.dateAdd",  // 11.d MoveRelativeDate
]);  // (11.g.iii MoveRelativeDate not called because days already balanced)
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "weeks" }));
assert.compareArray(actual.slice(expected.length), expectedOpsForWeekRounding.slice(expected.length), "order of operations with smallestUnit = weeks");
actual.slice(0, actual.length); // clear
