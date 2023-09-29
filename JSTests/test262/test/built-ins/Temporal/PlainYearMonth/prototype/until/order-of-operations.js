// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: Properties on objects passed to until() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalYearMonth
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
  "get other.calendar.fields",
  "call other.calendar.fields",
  "get other.month",
  "get other.month.valueOf",
  "call other.month.valueOf",
  "get other.monthCode",
  "get other.monthCode.toString",
  "call other.monthCode.toString",
  "get other.year",
  "get other.year.valueOf",
  "call other.year.valueOf",
  "get other.calendar.yearMonthFromFields",
  "call other.calendar.yearMonthFromFields",
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
  // CalendarFields
  "get this.calendar.fields",
  "call this.calendar.fields",
  // PrepareTemporalFields / CalendarDateFromFields (receiver)
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  "get this.calendar.dateFromFields",
  "call this.calendar.dateFromFields",
  // PrepareTemporalFields / CalendarDateFromFields (argument)
  "get other.calendar.monthCode",
  "call other.calendar.monthCode",
  "get other.calendar.year",
  "call other.calendar.year",
  "get this.calendar.dateFromFields",
  "call this.calendar.dateFromFields",
  // CalendarDateUntil
  "get this.calendar.dateUntil",
  "call this.calendar.dateUntil",
];
const actual = [];

const ownCalendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainYearMonth(2000, 5, ownCalendar, 1);

const otherYearMonthPropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 5,
  monthCode: "M05",
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
}, "other");

function createOptionsObserver({ smallestUnit = "months", largestUnit = "auto", roundingMode = "halfExpand", roundingIncrement = 1 } = {}) {
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

// code path that skips RoundDuration:
instance.since(otherYearMonthPropertyBag, createOptionsObserver({ smallestUnit: "months", roundingIncrement: 1 }));
assert.compareArray(actual, expected, "order of operations with no rounding");
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
instance.until(otherYearMonthPropertyBag, createOptionsObserver({ smallestUnit: "years" }));
assert.compareArray(actual, expectedOpsForYearRounding, "order of operations with smallestUnit = years");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest month:
const expectedOpsForMonthRounding = expected.concat([
  "get this.calendar.dateAdd",     // 10.b
  "call this.calendar.dateAdd",    // 10.c
  "call this.calendar.dateAdd",    // 10.e
  "call this.calendar.dateAdd",    // 10.k MoveRelativeDate
]);  // (10.n.iii MoveRelativeDate not called because weeks == 0)
instance.until(otherYearMonthPropertyBag, createOptionsObserver({ smallestUnit: "months", roundingIncrement: 2 }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with smallestUnit = months");
actual.splice(0); // clear
