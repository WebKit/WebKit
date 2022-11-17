// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: Properties on objects passed to round() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
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
  // ToRelativeTemporalObject
  "get options.relativeTo",
  "get options.relativeTo.calendar",
  "has options.relativeTo.calendar.calendar",
  "get options.relativeTo.calendar.fields",
  "call options.relativeTo.calendar.fields",
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
  "get options.relativeTo.second",
  "get options.relativeTo.second.valueOf",
  "call options.relativeTo.second.valueOf",
  "get options.relativeTo.year",
  "get options.relativeTo.year.valueOf",
  "call options.relativeTo.year.valueOf",
  "get options.relativeTo.calendar.dateFromFields",
  "call options.relativeTo.calendar.dateFromFields",
  "get options.relativeTo.offset",
  "get options.relativeTo.timeZone",
];
const actual = [];

const relativeTo = TemporalHelpers.propertyBagObserver(actual, {
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
  calendar: TemporalHelpers.calendarObserver(actual, "options.relativeTo.calendar"),
}, "options.relativeTo");

function createOptionsObserver({ smallestUnit = "nanoseconds", largestUnit = "auto", roundingMode = "halfExpand", roundingIncrement = 1, relativeTo = undefined } = {}) {
  return TemporalHelpers.propertyBagObserver(actual, {
    smallestUnit,
    largestUnit,
    roundingMode,
    roundingIncrement,
    relativeTo,
  }, "options");
}

const instance = new Temporal.Duration(0, 0, 0, 0, /* hours = */ 2400);

// basic order of observable operations, without rounding:
instance.round(createOptionsObserver({ relativeTo }));
assert.compareArray(actual, expected, "order of operations");
actual.splice(0, actual.length); // clear

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expected.concat([
  "get options.relativeTo.calendar.dateAdd",     // 9.b
  "call options.relativeTo.calendar.dateAdd",    // 9.c
  "call options.relativeTo.calendar.dateAdd",    // 9.e
  "call options.relativeTo.calendar.dateAdd",    // 9.j
  "get options.relativeTo.calendar.dateUntil",   // 9.m
  "call options.relativeTo.calendar.dateUntil",  // 9.m
  "call options.relativeTo.calendar.dateAdd",    // 9.r
  "call options.relativeTo.calendar.dateAdd",    // 9.w MoveRelativeDate
]);
instance.round(createOptionsObserver({ smallestUnit: "years", relativeTo }));
assert.compareArray(actual, expectedOpsForYearRounding, "order of operations with smallestUnit = years");
actual.splice(0, actual.length); // clear

// code path through Duration.prototype.round that rounds to the nearest month:
const expectedOpsForMonthRounding = expected.concat([
  // UnbalanceDurationRelative
  "get options.relativeTo.calendar.dateAdd",     // 9.b
  "get options.relativeTo.calendar.dateUntil",   // 9.c
  "call options.relativeTo.calendar.dateAdd",    // 9.d.i
  "call options.relativeTo.calendar.dateUntil",  // 9.d.iv
  // RoundDuration
  "get options.relativeTo.calendar.dateAdd",     // 10.b
  "call options.relativeTo.calendar.dateAdd",    // 10.c
  "call options.relativeTo.calendar.dateAdd",    // 10.e
  "call options.relativeTo.calendar.dateAdd",    // 10.k MoveRelativeDate
], Array(2).fill("call options.relativeTo.calendar.dateAdd"), [ // 2× 10.n.iii MoveRelativeDate
  // BalanceDurationRelative
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
]);
const instance2 = new Temporal.Duration(1, 0, 0, 62);
instance2.round(createOptionsObserver({ largestUnit: "months", smallestUnit: "months", relativeTo }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with largestUnit = smallestUnit = months");
actual.splice(0, actual.length); // clear

// code path through Duration.prototype.round that rounds to the nearest week:
const expectedOpsForWeekRounding = expected.concat([
  // UnbalanceDurationRelative
  "get options.relativeTo.calendar.dateAdd",   // 10.b
  "call options.relativeTo.calendar.dateAdd",  // 10.c.i MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 10.d.i MoveRelativeDate
  // RoundDuration
  "get options.relativeTo.calendar.dateAdd",   // 11.c
  "call options.relativeTo.calendar.dateAdd",  // 11.d MoveRelativeDate
], Array(58).fill("call options.relativeTo.calendar.dateAdd"), [  // 58× 11.g.iii MoveRelativeDate (52 + 4 + 2)
  // BalanceDurationRelative
  "get options.relativeTo.calendar.dateAdd",   // 12.b
  "call options.relativeTo.calendar.dateAdd",  // 12.c
]);
const instance3 = new Temporal.Duration(1, 1, 0, 15);
instance3.round(createOptionsObserver({ largestUnit: "weeks", smallestUnit: "weeks", relativeTo }));
assert.compareArray(actual, expectedOpsForWeekRounding, "order of operations with largestUnit = smallestUnit = weeks");
actual.splice(0, actual.length);  // clear

// code path through UnbalanceDurationRelative that rounds to the nearest day:
const expectedOpsForDayRounding = expected.concat([
  "get options.relativeTo.calendar.dateAdd",   // 11.a.ii
  "call options.relativeTo.calendar.dateAdd",  // 11.a.iii.1 MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 11.a.iv.1 MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 11.a.v.1 MoveRelativeDate
]);
const instance4 = new Temporal.Duration(1, 1, 1)
instance4.round(createOptionsObserver({ largestUnit: "days", smallestUnit: "days", relativeTo }));
assert.compareArray(actual, expectedOpsForDayRounding, "order of operations with largestUnit = smallestUnit = days");
actual.splice(0, actual.length);  // clear

// code path through BalanceDurationRelative balancing from days up to years:
const expectedOpsForDayToYearBalancing = expected.concat([
  "get options.relativeTo.calendar.dateAdd",     // 10.a
  "call options.relativeTo.calendar.dateAdd",    // 10.b MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 10.e.iv MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 10.f MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 10.i.iv MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 10.j
  "get options.relativeTo.calendar.dateUntil",   // 10.k
  "call options.relativeTo.calendar.dateUntil",  // 10.n
]);
const instance5 = new Temporal.Duration(0, 0, 0, 0, /* hours = */ 396 * 24);
instance5.round(createOptionsObserver({ largestUnit: "years", smallestUnit: "days", relativeTo }));
assert.compareArray(actual, expectedOpsForDayToYearBalancing, "order of operations with largestUnit = years, smallestUnit = days");
actual.splice(0, actual.length);  // clear

// code path through Duration.prototype.round balancing from months up to years:
const expectedOpsForMonthToYearBalancing = expected.concat([
  // RoundDuration
  "get options.relativeTo.calendar.dateAdd",     // 10.b
  "call options.relativeTo.calendar.dateAdd",    // 10.c
  "call options.relativeTo.calendar.dateAdd",    // 10.e
  "call options.relativeTo.calendar.dateAdd",    // 10.k MoveRelativeDate
  // BalanceDurationRelative
  "get options.relativeTo.calendar.dateAdd",     // 10.a
  "call options.relativeTo.calendar.dateAdd",    // 10.b MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 10.f MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 10.j
  "get options.relativeTo.calendar.dateUntil",   // 10.k
  "call options.relativeTo.calendar.dateUntil",  // 10.n
  "call options.relativeTo.calendar.dateAdd",    // 10.p.iv
  "call options.relativeTo.calendar.dateUntil",  // 10.p.vii
]);
const instance6 = new Temporal.Duration(0, 12);
instance6.round(createOptionsObserver({ largestUnit: "years", smallestUnit: "months", relativeTo }));
assert.compareArray(actual, expectedOpsForMonthToYearBalancing, "order of operations with largestUnit = years, smallestUnit = months");
actual.splice(0, actual.length); // clear

const expectedOpsForDayToMonthBalancing = expected.concat([
  // BalanceDurationRelative
  "get options.relativeTo.calendar.dateAdd",     // 11.a
  "call options.relativeTo.calendar.dateAdd",    // 11.b MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 11.e.iv MoveRelativeDate
]);
const instance7 = new Temporal.Duration(0, 0, 0, 0, /* hours = */ 32 * 24);
instance7.round(createOptionsObserver({ largestUnit: "months", smallestUnit: "days", relativeTo }));
assert.compareArray(actual, expectedOpsForDayToMonthBalancing, "order of operations with largestUnit = months, smallestUnit = days");
actual.splice(0, actual.length); // clear

const expectedOpsForDayToWeekBalancing = expected.concat([
  // BalanceDurationRelative
  "get options.relativeTo.calendar.dateAdd",   // 12.b
  "call options.relativeTo.calendar.dateAdd",  // 12.c MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 12.f.iv MoveRelativeDate
]);
const instance8 = new Temporal.Duration(0, 0, 0, 0, /* hours = */ 8 * 24);
instance8.round(createOptionsObserver({ largestUnit: "weeks", smallestUnit: "days", relativeTo }));
assert.compareArray(actual, expectedOpsForDayToWeekBalancing, "order of operations with largestUnit = weeks, smallestUnit = days");
actual.splice(0, actual.length); // clear
