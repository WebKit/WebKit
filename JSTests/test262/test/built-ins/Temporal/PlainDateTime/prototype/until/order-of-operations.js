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
  "get other.calendar.dateFromFields",
  "call other.calendar.dateFromFields",
  // CalendarEquals
  "get this.calendar[Symbol.toPrimitive]",
  "get this.calendar.toString",
  "call this.calendar.toString",
  "get other.calendar[Symbol.toPrimitive]",
  "get other.calendar.toString",
  "call other.calendar.toString",
  // GetDifferenceSettings
  "get options.largestUnit",
  "get options.largestUnit.toString",
  "call options.largestUnit.toString",
  "get options.roundingIncrement",
  "get options.roundingIncrement.valueOf",
  "call options.roundingIncrement.valueOf",
  "get options.roundingMode",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
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
  // CalendarDateUntil
  "get this.calendar.dateUntil",
  "call this.calendar.dateUntil",
];
const actual = [];

const ownCalendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, ownCalendar);

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

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expected.concat([
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
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest month:
const expectedOpsForMonthRounding = expected.concat([
  "get this.calendar.dateAdd",     // 10.b
  "call this.calendar.dateAdd",    // 10.c
  "call this.calendar.dateAdd",    // 10.e
  "call this.calendar.dateAdd",    // 10.k MoveRelativeDate
]);  // (10.n.iii MoveRelativeDate not called because weeks == 0)
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "months" }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with smallestUnit = years");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest week:
const expectedOpsForWeekRounding = expected.concat([
  "get this.calendar.dateAdd",   // 11.c
  "call this.calendar.dateAdd",  // 11.d MoveRelativeDate
]);  // (11.g.iii MoveRelativeDate not called because days already balanced)
instance.until(otherDateTimePropertyBag, createOptionsObserver({ smallestUnit: "weeks" }));
assert.compareArray(actual.slice(expected.length), expectedOpsForWeekRounding.slice(expected.length), "order of operations with smallestUnit = weeks");
