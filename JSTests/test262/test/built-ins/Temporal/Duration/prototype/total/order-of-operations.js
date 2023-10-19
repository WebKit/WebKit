// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: Properties on objects passed to total() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get options.relativeTo",
  "get options.unit",
  "get options.unit.toString",
  "call options.unit.toString",
];
const actual = [];

function createOptionsObserver({ unit = "nanoseconds", roundingMode = "halfExpand", roundingIncrement = 1, relativeTo = undefined } = {}) {
  return TemporalHelpers.propertyBagObserver(actual, {
    unit,
    roundingMode,
    roundingIncrement,
    relativeTo,
  }, "options");
}

const instance = new Temporal.Duration(0, 0, 0, 0, 2400);

// basic order of observable operations, with no relativeTo
instance.total(createOptionsObserver({ unit: "nanoseconds" }));
assert.compareArray(actual, expected, "order of operations");
actual.splice(0); // clear

const expectedOpsForPlainRelativeTo = [
  // ToRelativeTemporalObject
  "get options.relativeTo",
  "get options.relativeTo.calendar",
  "has options.relativeTo.calendar.dateAdd",
  "has options.relativeTo.calendar.dateFromFields",
  "has options.relativeTo.calendar.dateUntil",
  "has options.relativeTo.calendar.day",
  "has options.relativeTo.calendar.dayOfWeek",
  "has options.relativeTo.calendar.dayOfYear",
  "has options.relativeTo.calendar.daysInMonth",
  "has options.relativeTo.calendar.daysInWeek",
  "has options.relativeTo.calendar.daysInYear",
  "has options.relativeTo.calendar.fields",
  "has options.relativeTo.calendar.id",
  "has options.relativeTo.calendar.inLeapYear",
  "has options.relativeTo.calendar.mergeFields",
  "has options.relativeTo.calendar.month",
  "has options.relativeTo.calendar.monthCode",
  "has options.relativeTo.calendar.monthDayFromFields",
  "has options.relativeTo.calendar.monthsInYear",
  "has options.relativeTo.calendar.weekOfYear",
  "has options.relativeTo.calendar.year",
  "has options.relativeTo.calendar.yearMonthFromFields",
  "has options.relativeTo.calendar.yearOfWeek",
  "get options.relativeTo.calendar.fields",
  "call options.relativeTo.calendar.fields",
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
  "get options.relativeTo.calendar.dateFromFields",
  "call options.relativeTo.calendar.dateFromFields",
  // GetTemporalUnit
  "get options.unit",
  "get options.unit.toString",
  "call options.unit.toString",
];

const plainRelativeTo = TemporalHelpers.propertyBagObserver(actual, {
  year: 2001,
  month: 5,
  monthCode: "M05",
  day: 2,
  calendar: TemporalHelpers.calendarObserver(actual, "options.relativeTo.calendar"),
}, "options.relativeTo");

// basic order of observable operations, without rounding:
instance.total(createOptionsObserver({ unit: "nanoseconds", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForPlainRelativeTo, "order of operations for PlainDate relativeTo");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year with minimal calendar calls:
const expectedOpsForMinimalYearRounding = expectedOpsForPlainRelativeTo.concat([
  "get options.relativeTo.calendar.dateAdd",     // 7.c
  // 7.e and 7.g not called because years, months, weeks are 0
  "get options.relativeTo.calendar.dateUntil",   // 7.o
  "call options.relativeTo.calendar.dateUntil",  // 7.o
  // 7.s not called because years, months, weeks are 0
  "call options.relativeTo.calendar.dateAdd",    // 7.y MoveRelativeDate
]);
instance.total(createOptionsObserver({ unit: "years", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForMinimalYearRounding, "order of operations with years = 0 and unit = years");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expectedOpsForPlainRelativeTo.concat([
  "get options.relativeTo.calendar.dateAdd",     // 7.c
  "call options.relativeTo.calendar.dateAdd",    // 7.d
  "call options.relativeTo.calendar.dateAdd",    // 7.f
  "get options.relativeTo.calendar.dateUntil",   // 7.n
  "call options.relativeTo.calendar.dateUntil",  // 7.n
  "call options.relativeTo.calendar.dateAdd",    // 7.s
  "call options.relativeTo.calendar.dateAdd",    // 7.x MoveRelativeDate
]);
const instanceYears = new Temporal.Duration(1, 12, 0, 0, /* hours = */ 2400);
instanceYears.total(createOptionsObserver({ unit: "years", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForYearRounding, "order of operations with unit = years");
actual.splice(0); // clear

// code path through Duration.prototype.total that rounds to the nearest month:
const expectedOpsForMonthRounding = expectedOpsForPlainRelativeTo.concat([
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
], Array(2).fill("call options.relativeTo.calendar.dateAdd")); // 2× 10.n.iii MoveRelativeDate
const instance2 = new Temporal.Duration(1, 0, 0, 62);
instance2.total(createOptionsObserver({ unit: "months", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with unit = months");
actual.splice(0); // clear

// code path through Duration.prototype.total that rounds to the nearest week:
const expectedOpsForWeekRounding = expectedOpsForPlainRelativeTo.concat([
  // UnbalanceDurationRelative
  "get options.relativeTo.calendar.dateAdd",   // 10.b
  "call options.relativeTo.calendar.dateAdd",  // 10.c.i MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 10.d.i MoveRelativeDate
  // RoundDuration
  "get options.relativeTo.calendar.dateAdd",   // 11.c
  "call options.relativeTo.calendar.dateAdd",  // 11.d MoveRelativeDate
], Array(58).fill("call options.relativeTo.calendar.dateAdd"));  // 58× 11.g.iii MoveRelativeDate (52 + 4 + 2)
const instance3 = new Temporal.Duration(1, 1, 0, 15);
instance3.total(createOptionsObserver({ unit: "weeks", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForWeekRounding, "order of operations with unit = weeks");
actual.splice(0); // clear

// code path through UnbalanceDurationRelative that rounds to the nearest day:
const expectedOpsForDayRounding = expectedOpsForPlainRelativeTo.concat([
  "get options.relativeTo.calendar.dateAdd",   // 11.a.ii
  "call options.relativeTo.calendar.dateAdd",  // 11.a.iii.1 MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 11.a.iv.1 MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 11.a.v.1 MoveRelativeDate
]);
const instance4 = new Temporal.Duration(1, 1, 1)
instance4.total(createOptionsObserver({ unit: "days", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForDayRounding, "order of operations with unit = days");
actual.splice(0);  // clear

const expectedOpsForZonedRelativeTo = [
  // ToRelativeTemporalObject
  "get options.relativeTo",
  "get options.relativeTo.calendar",
  "has options.relativeTo.calendar.dateAdd",
  "has options.relativeTo.calendar.dateFromFields",
  "has options.relativeTo.calendar.dateUntil",
  "has options.relativeTo.calendar.day",
  "has options.relativeTo.calendar.dayOfWeek",
  "has options.relativeTo.calendar.dayOfYear",
  "has options.relativeTo.calendar.daysInMonth",
  "has options.relativeTo.calendar.daysInWeek",
  "has options.relativeTo.calendar.daysInYear",
  "has options.relativeTo.calendar.fields",
  "has options.relativeTo.calendar.id",
  "has options.relativeTo.calendar.inLeapYear",
  "has options.relativeTo.calendar.mergeFields",
  "has options.relativeTo.calendar.month",
  "has options.relativeTo.calendar.monthCode",
  "has options.relativeTo.calendar.monthDayFromFields",
  "has options.relativeTo.calendar.monthsInYear",
  "has options.relativeTo.calendar.weekOfYear",
  "has options.relativeTo.calendar.year",
  "has options.relativeTo.calendar.yearMonthFromFields",
  "has options.relativeTo.calendar.yearOfWeek",
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
  "get options.relativeTo.calendar.dateFromFields",
  "call options.relativeTo.calendar.dateFromFields",
  "has options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "has options.relativeTo.timeZone.getPossibleInstantsFor",
  "has options.relativeTo.timeZone.id",
  // InterpretISODateTimeOffset
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // GetTemporalUnit
  "get options.unit",
  "get options.unit.toString",
  "call options.unit.toString",
];

const zonedRelativeTo = TemporalHelpers.propertyBagObserver(actual, {
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
  offset: "+00:00",
  calendar: TemporalHelpers.calendarObserver(actual, "options.relativeTo.calendar"),
  timeZone: TemporalHelpers.timeZoneObserver(actual, "options.relativeTo.timeZone"),
}, "options.relativeTo");

// basic order of observable operations, without rounding:
instance.total(createOptionsObserver({ unit: "nanoseconds", relativeTo: zonedRelativeTo }));
assert.compareArray(actual, expectedOpsForZonedRelativeTo, "order of operations for ZonedDateTime relativeTo");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year with minimal calendar operations:
const expectedOpsForMinimalYearRoundingZoned = expectedOpsForZonedRelativeTo.concat([
  // ToTemporalDate
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // BalancePossiblyInfiniteDuration → NanosecondsToDays
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 7. GetPlainDateTimeFor
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 11. GetPlainDateTimeFor
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // BalancePossiblyInfiniteDuration → NanosecondsToDays → AddDaysToZonedDateTime
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // BalancePossiblyInfiniteDuration → NanosecondsToDays → AddDaysToZonedDateTime
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
], [
  // code path through RoundDuration that rounds to the nearest year:
  // MoveRelativeZonedDateTime → AddDaysToZonedDateTime
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  "get options.relativeTo.calendar.dateAdd",     // 7.c
  // 7.e and 7.g not called because years, months, weeks are 0
  "get options.relativeTo.calendar.dateUntil",   // 7.o
  "call options.relativeTo.calendar.dateUntil",  // 7.o
  // 7.s not called because years, months, weeks are 0
  "call options.relativeTo.calendar.dateAdd",    // 7.y MoveRelativeDate
]);
instance.total(createOptionsObserver({ unit: "years", relativeTo: zonedRelativeTo }));
assert.compareArray(
  actual,
  expectedOpsForMinimalYearRoundingZoned,
  "order of operations with years = 0, unit = years and ZonedDateTime relativeTo"
);
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRoundingZoned = expectedOpsForZonedRelativeTo.concat([
  // ToTemporalDate
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // MoveRelativeZonedDateTime → AddZonedDateTime
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // BalancePossiblyInfiniteTimeDurationRelative → NanosecondsToDays
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 7. GetPlainDateTimeFor
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 11. GetPlainDateTimeFor
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // BalancePossiblyInfiniteTimeDurationRelative → NanosecondsToDays → AddDaysToZonedDateTime
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // BalancePossiblyInfiniteTimeDurationRelative → NanosecondsToDays → AddDaysToZonedDateTime
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // RoundDuration → MoveRelativeZonedDateTime → AddZonedDateTime
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // RoundDuration
  "get options.relativeTo.calendar.dateAdd",     // 7.c
  "call options.relativeTo.calendar.dateAdd",    // 7.e
  "call options.relativeTo.calendar.dateAdd",    // 7.g
  "get options.relativeTo.calendar.dateUntil",   // 7.o
  "call options.relativeTo.calendar.dateUntil",  // 7.o
  "call options.relativeTo.calendar.dateAdd",    // 7.s MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 7.y MoveRelativeDate
]);
instanceYears.total(createOptionsObserver({ unit: "years", relativeTo: zonedRelativeTo }));
assert.compareArray(
  actual,
  expectedOpsForYearRoundingZoned,
  "order of operations with unit = years and ZonedDateTime relativeTo"
);
actual.splice(0); // clear

// code path that hits UnbalanceDateDurationRelative and RoundDuration
const expectedOpsForUnbalanceRound = expectedOpsForZonedRelativeTo.concat([
  // ToTemporalDate
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // No user code calls in UnbalanceDateDurationRelative
  // MoveRelativeZonedDateTime → AddZonedDateTime
  "get options.relativeTo.calendar.dateAdd",                  // 8.
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",   // 10. GetInstantFor
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // RoundDuration → MoveRelativeZonedDateTime → AddZonedDateTime
  "get options.relativeTo.calendar.dateAdd",                  // 8.
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",   // 10. GetInstantFor
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // RoundDuration
  "get options.relativeTo.calendar.dateAdd",   // 7.d.i
  "call options.relativeTo.calendar.dateAdd",  // 7.f
  "call options.relativeTo.calendar.dateAdd",  // 7.h
  "call options.relativeTo.calendar.dateAdd",  // 7.n MoveRelativeDate
]);
new Temporal.Duration(0, 1, 1).total(createOptionsObserver({ unit: "months", relativeTo: zonedRelativeTo }));
assert.compareArray(
  actual,
  expectedOpsForUnbalanceRound,
  "order of operations with unit = months and ZonedDateTime relativeTo"
);
actual.splice(0); // clear

// code path that avoids converting Zoned twice in BalanceTimeDurationRelative
const expectedOpsForBalanceRound = expectedOpsForZonedRelativeTo.concat([
  // ToTemporalDate
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // No user code calls in UnbalanceDateDurationRelative
  // No user code calls in AddZonedDateTime (years, months, weeks = 0)
  // BalanceTimeDurationRelative
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 4.a
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",   // 4.b
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",  // NanosecondsToDays 9
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",   // NanosecondsToDays 26
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",   // NanosecondsToDays 31.a
  // RoundDuration → MoveRelativeZonedDateTime → AddZonedDateTime
  "get options.relativeTo.timeZone.getPossibleInstantsFor",   // 10. GetInstantFor
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // RoundDuration
  "get options.relativeTo.calendar.dateAdd",   // 10.d.i
  "call options.relativeTo.calendar.dateAdd",  // 10.f
  "call options.relativeTo.calendar.dateAdd",  // 10.i.iii
]);
new Temporal.Duration(0, 0, 0, 1, 240).total(createOptionsObserver({ unit: "weeks", relativeTo: zonedRelativeTo }));
assert.compareArray(
  actual,
  expectedOpsForBalanceRound,
  "order of operations with unit = weeks and no calendar units"
);
actual.splice(0);  // clear
