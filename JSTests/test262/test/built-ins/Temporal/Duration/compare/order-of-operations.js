// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: Properties on objects passed to compare() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalDuration on first argument
  "get one.days",
  "get one.days.valueOf",
  "call one.days.valueOf",
  "get one.hours",
  "get one.hours.valueOf",
  "call one.hours.valueOf",
  "get one.microseconds",
  "get one.microseconds.valueOf",
  "call one.microseconds.valueOf",
  "get one.milliseconds",
  "get one.milliseconds.valueOf",
  "call one.milliseconds.valueOf",
  "get one.minutes",
  "get one.minutes.valueOf",
  "call one.minutes.valueOf",
  "get one.months",
  "get one.months.valueOf",
  "call one.months.valueOf",
  "get one.nanoseconds",
  "get one.nanoseconds.valueOf",
  "call one.nanoseconds.valueOf",
  "get one.seconds",
  "get one.seconds.valueOf",
  "call one.seconds.valueOf",
  "get one.weeks",
  "get one.weeks.valueOf",
  "call one.weeks.valueOf",
  "get one.years",
  "get one.years.valueOf",
  "call one.years.valueOf",
  // ToTemporalDuration on second argument
  "get two.days",
  "get two.days.valueOf",
  "call two.days.valueOf",
  "get two.hours",
  "get two.hours.valueOf",
  "call two.hours.valueOf",
  "get two.microseconds",
  "get two.microseconds.valueOf",
  "call two.microseconds.valueOf",
  "get two.milliseconds",
  "get two.milliseconds.valueOf",
  "call two.milliseconds.valueOf",
  "get two.minutes",
  "get two.minutes.valueOf",
  "call two.minutes.valueOf",
  "get two.months",
  "get two.months.valueOf",
  "call two.months.valueOf",
  "get two.nanoseconds",
  "get two.nanoseconds.valueOf",
  "call two.nanoseconds.valueOf",
  "get two.seconds",
  "get two.seconds.valueOf",
  "call two.seconds.valueOf",
  "get two.weeks",
  "get two.weeks.valueOf",
  "call two.weeks.valueOf",
  "get two.years",
  "get two.years.valueOf",
  "call two.years.valueOf",
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
  "has options.relativeTo.timeZone.timeZone",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // CalculateOffsetShift on first argument
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // ...in AddZonedDateTime
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  // ...done with AddZonedDateTime
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  // CalculateOffsetShift on second argument
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",
  "call options.relativeTo.calendar.dateAdd",
  "get options.relativeTo.timeZone.getPossibleInstantsFor",
  "call options.relativeTo.timeZone.getPossibleInstantsFor",
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
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
  timeZone: TemporalHelpers.timeZoneObserver(actual, "options.relativeTo.timeZone"),
}, "options.relativeTo");

function createOptionsObserver(relativeTo = undefined) {
  return TemporalHelpers.propertyBagObserver(actual, { relativeTo }, "options");
}

function createDurationPropertyBagObserver(name, y = 0, mon = 0, w = 0, d = 0, h = 0, min = 0, s = 0, ms = 0, µs = 0, ns = 0) {
  return TemporalHelpers.propertyBagObserver(actual, {
    years: y,
    months: mon,
    weeks: w,
    days: d,
    hours: h,
    minutes: min,
    seconds: s,
    milliseconds: ms,
    microseconds: µs,
    nanoseconds: ns,
  }, name);
}

// basic order of observable operations, without
Temporal.Duration.compare(
  createDurationPropertyBagObserver("one", 0, 0, 0, 7),
  createDurationPropertyBagObserver("two", 0, 0, 0, 6),
  createOptionsObserver(relativeTo)
);
assert.compareArray(actual, expected, "order of operations");
actual.splice(0, actual.length); // clear

// code path through UnbalanceDurationRelative that balances higher units down
// to days:
const expectedOpsForDayBalancing = expected.concat([
  // UnbalanceDurationRelative
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 7.a ToTemporalDate
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",   // 11.a.ii
  "call options.relativeTo.calendar.dateAdd",  // 11.a.iii.1 MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 11.a.iv.1 MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 11.a.v.1 MoveRelativeDate
  // UnbalanceDurationRelative again for the second argument:
  "get options.relativeTo.timeZone.getOffsetNanosecondsFor",  // 7.a ToTemporalDate
  "call options.relativeTo.timeZone.getOffsetNanosecondsFor",
  "get options.relativeTo.calendar.dateAdd",   // 11.a.ii
  "call options.relativeTo.calendar.dateAdd",  // 11.a.iii.1 MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 11.a.iv.1 MoveRelativeDate
  "call options.relativeTo.calendar.dateAdd",  // 11.a.v.1 MoveRelativeDate
]);
Temporal.Duration.compare(
  createDurationPropertyBagObserver("one", 1, 1, 1),
  createDurationPropertyBagObserver("two", 1, 1, 1, 1),
  createOptionsObserver(relativeTo)
);
assert.compareArray(actual.slice(expected.length), expectedOpsForDayBalancing.slice(expected.length), "order of operations with calendar units");
actual.splice(0, actual.length);  // clear
