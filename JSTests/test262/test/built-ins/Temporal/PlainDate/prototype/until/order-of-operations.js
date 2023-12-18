// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Properties on objects passed to until() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expectedMinimal = [
  // ToTemporalDate
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
  "get other.day",
  "get other.day.valueOf",
  "call other.day.valueOf",
  "get other.month",
  "get other.month.valueOf",
  "call other.month.valueOf",
  "get other.monthCode",
  "get other.monthCode.toString",
  "call other.monthCode.toString",
  "get other.year",
  "get other.year.valueOf",
  "call other.year.valueOf",
  "get other.calendar.dateFromFields",
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

const expected = expectedMinimal.concat([
  // CalendarDateUntil
  "get this.calendar.dateUntil",
  "call this.calendar.dateUntil",
]);
const actual = [];

const ownCalendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.PlainDate(2000, 5, 2, ownCalendar);

const otherDatePropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 6,
  monthCode: "M06",
  day: 2,
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
}, "other");

function createOptionsObserver({ smallestUnit = "days", largestUnit = "auto", roundingMode = "halfExpand", roundingIncrement = 1 } = {}) {
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
instance.until(otherDatePropertyBag, createOptionsObserver({ largestUnit: "years" }));
assert.compareArray(actual, expected, "order of operations");
actual.splice(0); // clear

// short-circuit for identical objects:

const identicalPropertyBag = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 5,
  monthCode: "M05",
  day: 2,
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
}, "other");

instance.since(identicalPropertyBag, createOptionsObserver());
assert.compareArray(actual, expectedMinimal, "order of operations with identical dates");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expected.concat([
  "get this.calendar.dateAdd",     // 7.c.i
  "call this.calendar.dateAdd",    // 7.e
  "call this.calendar.dateAdd",    // 7.g
  "get this.calendar.dateUntil",   // 7.o
  "call this.calendar.dateUntil",  // 7.o
  "call this.calendar.dateAdd",    // 7.y MoveRelativeDate
]);  // (7.s not called because other units can't add up to >1 year at this point)
instance.until(otherDatePropertyBag, createOptionsObserver({ smallestUnit: "years" }));
assert.compareArray(actual, expectedOpsForYearRounding, "order of operations with smallestUnit = years");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year and skips a DateUntil call:
const otherDatePropertyBagSameMonth = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 5,
  monthCode: "M05",
  day: 2,
  calendar: TemporalHelpers.calendarObserver(actual, "other.calendar"),
}, "other");
const expectedOpsForYearRoundingSameMonth = expected.concat([
  "get this.calendar.dateAdd",     // 7.c.i
  "call this.calendar.dateAdd",    // 7.e
  "call this.calendar.dateAdd",    // 7.g
  "call this.calendar.dateAdd",    // 7.y MoveRelativeDate
]);  // (7.o not called because months and weeks == 0)
instance.until(otherDatePropertyBagSameMonth, createOptionsObserver({ smallestUnit: "years" }));
assert.compareArray(actual, expectedOpsForYearRoundingSameMonth, "order of operations with smallestUnit = years and no excess months/weeks");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest month:
const expectedOpsForMonthRounding = expected.concat([
  "get this.calendar.dateAdd",     // 10.b
  "call this.calendar.dateAdd",    // 10.c
  "call this.calendar.dateAdd",    // 10.e
  "call this.calendar.dateAdd",    // 10.k MoveRelativeDate
]);  // (10.n.iii MoveRelativeDate not called because weeks == 0)
instance.until(otherDatePropertyBag, createOptionsObserver({ smallestUnit: "months" }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with smallestUnit = months");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest week:
const expectedOpsForWeekRounding = expected.concat([
  "get this.calendar.dateAdd",   // 11.c
  "call this.calendar.dateAdd",  // 11.d MoveRelativeDate
]);  // (11.g.iii MoveRelativeDate not called because days already balanced)
instance.until(otherDatePropertyBag, createOptionsObserver({ smallestUnit: "weeks" }));
assert.compareArray(actual.slice(expected.length), expectedOpsForWeekRounding.slice(expected.length), "order of operations with smallestUnit = weeks");
