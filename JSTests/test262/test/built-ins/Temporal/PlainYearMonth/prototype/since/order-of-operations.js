// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Properties on objects passed to since() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expectedMinimal = [
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
  "get other.calendar.yearMonthFromFields",
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
];

const expected = expectedMinimal.concat([
  // lookup
  "get this.calendar.dateAdd",
  "get this.calendar.dateFromFields",
  "get this.calendar.dateUntil",
  "get this.calendar.fields",
  // CalendarFields
  "call this.calendar.fields",
  // PrepareTemporalFields / CalendarDateFromFields (receiver)
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  "call this.calendar.dateFromFields",
  // PrepareTemporalFields / CalendarDateFromFields (argument)
  "get other.calendar.monthCode",
  "call other.calendar.monthCode",
  "get other.calendar.year",
  "call other.calendar.year",
  "call this.calendar.dateFromFields",
  // CalendarDateUntil
  "call this.calendar.dateUntil",
]);
const actual = [];

const ownCalendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainYearMonth(2000, 5, ownCalendar, 1);

const otherYearMonthPropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 6,
  monthCode: "M06",
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

// short-circuit for identical objects:
const identicalPropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 5,
  monthCode: "M05",
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
}, "other");

instance.since(identicalPropertyBag, createOptionsObserver());
assert.compareArray(actual, expectedMinimal, "order of operations with identical year-months");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expected.concat([
  // RoundDuration
  "call this.calendar.dateAdd",    // 7.e
  "call this.calendar.dateAdd",    // 7.g
  "call this.calendar.dateUntil",  // 7.o
  "call this.calendar.dateAdd",    // 7.y MoveRelativeDate
  // (7.s not called because other units can't add up to >1 year at this point)
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 9.c
  "call this.calendar.dateUntil"   // 9.d
]);
instance.since(otherYearMonthPropertyBag, createOptionsObserver({ smallestUnit: "years" }));
assert.compareArray(actual, expectedOpsForYearRounding, "order of operations with smallestUnit = years");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year and skips a DateUntil call
const otherYearMonthPropertyBagSameMonth = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 5,
  monthCode: "M05",
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
}, "other");
const expectedOpsForYearRoundingSameMonth = expected.concat([
  "call this.calendar.dateAdd",    // 7.e
  "call this.calendar.dateAdd",    // 7.g
  "call this.calendar.dateAdd",    // 7.y MoveRelativeDate
  // (7.o not called because months and weeks == 0)
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 9.c
  "call this.calendar.dateUntil"   // 9.d
]);
instance.since(otherYearMonthPropertyBagSameMonth, createOptionsObserver({ smallestUnit: "years" }));
assert.compareArray(actual, expectedOpsForYearRoundingSameMonth, "order of operations with smallestUnit = years and no excess months");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest month:
const expectedOpsForMonthRounding = expected.concat([
  // RoundDuration
  "call this.calendar.dateAdd",    // 10.c
  "call this.calendar.dateAdd",    // 10.e
  "call this.calendar.dateAdd",    // 10.k MoveRelativeDate
  // (10.n.iii MoveRelativeDate not called because weeks == 0)
  // BalanceDateDurationRelative
  "call this.calendar.dateAdd",    // 10.d
  "call this.calendar.dateUntil"   // 10.e
]);
instance.since(otherYearMonthPropertyBag, createOptionsObserver({ smallestUnit: "months", roundingIncrement: 2 }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with smallestUnit = months");
