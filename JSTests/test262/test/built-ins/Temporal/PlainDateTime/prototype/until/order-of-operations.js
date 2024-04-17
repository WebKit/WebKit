// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: Properties on objects passed to until() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalDateTime
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
  "get other.second",
  "get other.second.valueOf",
  "call other.second.valueOf",
  "get other.year",
  "get other.year.valueOf",
  "call other.year.valueOf",
  "call other.calendar.dateFromFields",
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

const ownCalendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, ownCalendar);

const otherDateTimePropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 6,
  monthCode: "M06",
  day: 2,
  hour: 1,
  minute: 46,
  second: 40,
  millisecond: 250,
  microsecond: 500,
  nanosecond: 750,
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
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

// basic order of observable operations with calendar call, without rounding:
instance.until(otherDateTimePropertyBag, createOptionsObserver({ largestUnit: "years" }));
assert.compareArray(actual, expected.concat([
  // lookup
  "get this.calendar.dateAdd",
  "get this.calendar.dateUntil",
  // CalendarDateUntil
  "call this.calendar.dateUntil",
]), "order of operations");
actual.splice(0); // clear

// short-circuit for identical objects:
const identicalPropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 5,
  monthCode: "M05",
  day: 2,
  hour: 12,
  minute: 34,
  second: 56,
  millisecond: 987,
  microsecond: 654,
  nanosecond: 321,
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
}, "other");

instance.until(identicalPropertyBag, createOptionsObserver());
assert.compareArray(actual, expected, "order of operations with identical datetimes");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expected.concat([
  // lookup
  "get this.calendar.dateAdd",
  "get this.calendar.dateUntil",
  // CalendarDateUntil
  "call this.calendar.dateUntil",
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

// code path through RoundDuration that rounds to the nearest year and skips a DateUntil call:
const otherDatePropertyBagSameMonth = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 5,
  monthCode: "M05",
  day: 2,
  hour: 12,
  minute: 34,
  second: 56,
  millisecond: 987,
  microsecond: 654,
  nanosecond: 321,
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
}, "other");
const expectedOpsForYearRoundingSameMonth = expected.concat([
  // lookup
  "get this.calendar.dateAdd",
  "get this.calendar.dateUntil",
  // CalendarDateUntil
  "call this.calendar.dateUntil",
  // RoundDuration
  "call this.calendar.dateAdd",    // 12.d
  "call this.calendar.dateAdd",    // 12.f
  "call this.calendar.dateAdd",    // 12.x MoveRelativeDate
  // (12.n not called because months and weeks == 0)
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 9.c
  "call this.calendar.dateUntil"   // 9.d
]);
instance.until(otherDatePropertyBagSameMonth, createOptionsObserver({ smallestUnit: "years" }));
assert.compareArray(actual, expectedOpsForYearRoundingSameMonth, "order of operations with smallestUnit = years and no excess months/weeks");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest month:
const expectedOpsForMonthRounding = expected.concat([
  // lookup
  "get this.calendar.dateAdd",
  "get this.calendar.dateUntil",
  // CalendarDateUntil
  "call this.calendar.dateUntil",
  // RoundDuration
  "call this.calendar.dateAdd",    // 13.c
  "call this.calendar.dateAdd",    // 13.e
  "call this.calendar.dateUntil",  // 13.m
  "call this.calendar.dateAdd",    // 13.w MoveRelativeDate
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 10.d
  "call this.calendar.dateUntil"   // 10.e
]);
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "months" }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with smallestUnit = years");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest week:
const expectedOpsForWeekRounding = expected.concat([
  // lookup
  "get this.calendar.dateAdd",
  "get this.calendar.dateUntil",
  // CalendarDateUntil
  "call this.calendar.dateUntil",
  // RoundDuration
  "call this.calendar.dateUntil",  // 14.f
  "call this.calendar.dateAdd",    // 14.p MoveRelativeDate
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 16
  "call this.calendar.dateUntil"   // 17
]);
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "weeks" }));
assert.compareArray(actual, expectedOpsForWeekRounding, "order of operations with smallestUnit = weeks");
