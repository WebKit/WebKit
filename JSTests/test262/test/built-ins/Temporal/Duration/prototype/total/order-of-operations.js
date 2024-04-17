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
  "get options.relativeTo.calendar.dateFromFields",
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
  "call options.relativeTo.calendar.dateFromFields",
  // GetTemporalUnit
  "get options.unit",
  "get options.unit.toString",
  "call options.unit.toString",
  // lookup in Duration.p.total
  "get options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.calendar.dateUntil",
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
  // 12.d and 12.f not called because years, months, weeks are 0
  "call options.relativeTo.calendar.dateUntil",  // 12.n
  // 12.r not called because years, months, weeks are 0
  "call options.relativeTo.calendar.dateAdd",    // 12.x MoveRelativeDate
]);
instance.total(createOptionsObserver({ unit: "years", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForMinimalYearRounding, "order of operations with years = 0 and unit = years");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year:
const expectedOpsForYearRounding = expectedOpsForPlainRelativeTo.concat([
  "call options.relativeTo.calendar.dateAdd",    // 12.d
  "call options.relativeTo.calendar.dateAdd",    // 12.f
  "call options.relativeTo.calendar.dateUntil",  // 12.n
  "call options.relativeTo.calendar.dateAdd",    // 12.r MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 12.x MoveRelativeDate
]);
const instanceYears = new Temporal.Duration(1, 12, 0, 0, /* hours = */ 2400);
instanceYears.total(createOptionsObserver({ unit: "years", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForYearRounding, "order of operations with unit = years");
actual.splice(0); // clear

// code path through Duration.prototype.total that rounds to the nearest month:
const expectedOpsForMonthRounding = expectedOpsForPlainRelativeTo.concat([
  // UnbalanceDateDurationRelative
  "call options.relativeTo.calendar.dateAdd",    // 3.f
  "call options.relativeTo.calendar.dateUntil",  // 3.i
  // RoundDuration
  "call options.relativeTo.calendar.dateAdd",    // 13.c
  "call options.relativeTo.calendar.dateAdd",    // 13.e
  "call options.relativeTo.calendar.dateUntil",  // 13.m
  "call options.relativeTo.calendar.dateAdd",    // 13.q MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 13.w MoveRelativeDate
]);
const instance2 = new Temporal.Duration(1, 0, 0, 62);
instance2.total(createOptionsObserver({ unit: "months", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForMonthRounding, "order of operations with unit = months");
actual.splice(0); // clear

// code path through Duration.prototype.total that rounds to the nearest week:
const expectedOpsForWeekRounding = expectedOpsForPlainRelativeTo.concat([
  // UnbalanceDateDurationRelative
  "call options.relativeTo.calendar.dateAdd",  // 4.e
  // RoundDuration
  "call options.relativeTo.calendar.dateUntil",  // 14.f
  "call options.relativeTo.calendar.dateAdd",    // 14.j MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 14.p MoveRelativeDate
]);
const instance3 = new Temporal.Duration(1, 1, 0, 15);
instance3.total(createOptionsObserver({ unit: "weeks", relativeTo: plainRelativeTo }));
assert.compareArray(actual, expectedOpsForWeekRounding, "order of operations with unit = weeks");
actual.splice(0); // clear

// code path through UnbalanceDateDurationRelative that rounds to the nearest day:
const expectedOpsForDayRounding = expectedOpsForPlainRelativeTo.concat([
  "call options.relativeTo.calendar.dateAdd",  // 10
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
  "get options.relativeTo.calendar.dateFromFields",
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
  "call options.relativeTo.calendar.dateFromFields",
  "has options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "has options.relativeTo.timeZone.getPossibleInstantsFor",
  "has options.relativeTo.timeZone.id",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  // InterpretISODateTimeOffset
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
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
assert.compareArray(actual, expectedOpsForZonedRelativeTo.concat([
  "get options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.calendar.dateUntil",
]), "order of operations for ZonedDateTime relativeTo");
actual.splice(0); // clear

// code path through RoundDuration that rounds to the nearest year with minimal calendar operations:
const expectedOpsForMinimalYearRoundingZoned = expectedOpsForZonedRelativeTo.concat([
  // ToTemporalDate
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // lookup in Duration.p.total
  "get options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.calendar.dateUntil",
  // BalancePossiblyInfiniteDuration → NanosecondsToDays
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 7. GetPlainDateTimeFor
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 11. GetPlainDateTimeFor
  // BalancePossiblyInfiniteDuration → NanosecondsToDays → AddDaysToZonedDateTime
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // BalancePossiblyInfiniteDuration → NanosecondsToDays → AddDaysToZonedDateTime
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
], [
  // code path through RoundDuration that rounds to the nearest year:
  // MoveRelativeZonedDateTime → AddDaysToZonedDateTime
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // 12.d and 12.f not called because years, months, weeks are 0
  "call options.relativeTo.calendar.dateUntil",  // 12.n
  // 12.r not called because years, months, weeks are 0
  "call options.relativeTo.calendar.dateAdd",    // 12.x MoveRelativeDate
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
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // lookup in Duration.p.total
  "get options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.calendar.dateUntil",
  // MoveRelativeZonedDateTime → AddZonedDateTime
  "call options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // BalancePossiblyInfiniteTimeDurationRelative → NanosecondsToDays
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 8. GetPlainDateTimeFor
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 9. GetPlainDateTimeFor
  // BalancePossiblyInfiniteTimeDurationRelative → NanosecondsToDays → AddDaysToZonedDateTime
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // BalancePossiblyInfiniteTimeDurationRelative → NanosecondsToDays → AddDaysToZonedDateTime
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // RoundDuration → MoveRelativeZonedDateTime → AddZonedDateTime
  "call options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // RoundDuration
  "call options.relativeTo.calendar.dateAdd",    // 12.d
  "call options.relativeTo.calendar.dateAdd",    // 12.f
  "call options.relativeTo.calendar.dateUntil",  // 12.n
  "call options.relativeTo.calendar.dateAdd",    // 12.r MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 12.x MoveRelativeDate
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
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // lookup in Duration.p.total
  "get options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.calendar.dateUntil",
  // No user code calls in UnbalanceDateDurationRelative
  // MoveRelativeZonedDateTime → AddZonedDateTime
  "call options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",   // 13. GetInstantFor
  // RoundDuration → MoveRelativeZonedDateTime → AddZonedDateTime
  "call options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",   // 13. GetInstantFor
  // RoundDuration
  "call options.relativeTo.calendar.dateAdd",    // 13.c
  "call options.relativeTo.calendar.dateAdd",    // 13.e
  "call options.relativeTo.calendar.dateUntil",  // 13.m
  "call options.relativeTo.calendar.dateAdd",    // 13.w MoveRelativeDate
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
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // lookup in Duration.p.total
  "get options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.calendar.dateUntil",
  // No user code calls in UnbalanceDateDurationRelative
  // No user code calls in AddZonedDateTime (years, months, weeks = 0)
  // BalanceTimeDurationRelative
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 4.a
  "call options.relativeTo.timeZone.getPossibleInstantsFor",   // 4.b
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",  // NanosecondsToDays 9
  "call options.relativeTo.timeZone.getPossibleInstantsFor",   // NanosecondsToDays 26
  "call options.relativeTo.timeZone.getPossibleInstantsFor",   // NanosecondsToDays 31.a
  // RoundDuration → MoveRelativeZonedDateTime → AddZonedDateTime
  "call options.relativeTo.timeZone.getPossibleInstantsFor",  // 10. GetInstantFor
  // RoundDuration
  "call options.relativeTo.calendar.dateUntil",  // 14.f
  "call options.relativeTo.calendar.dateAdd",    // 14.j MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",    // 14.p MoveRelativeDate
]);
new Temporal.Duration(0, 0, 0, 1, 240).total(createOptionsObserver({ unit: "weeks", relativeTo: zonedRelativeTo }));
assert.compareArray(
  actual,
  expectedOpsForBalanceRound,
  "order of operations with unit = weeks and no calendar units"
);
actual.splice(0);  // clear
