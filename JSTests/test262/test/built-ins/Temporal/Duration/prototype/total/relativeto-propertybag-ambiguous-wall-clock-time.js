// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
  Correct time zone calls are made when converting a ZonedDateTime-like
  relativeTo property bag denoting an ambiguous wall-clock time
includes: [temporalHelpers.js, compareArray.js]
features: [Temporal]
---*/

const actual = [];

const dstTimeZone = TemporalHelpers.springForwardFallBackTimeZone();
const dstTimeZoneObserver = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  getOffsetNanosecondsFor: dstTimeZone.getOffsetNanosecondsFor.bind(dstTimeZone),
  getPossibleInstantsFor: dstTimeZone.getPossibleInstantsFor.bind(dstTimeZone),
});
const calendar = TemporalHelpers.calendarObserver(actual, "calendar");

const instance = new Temporal.Duration(1, 0, 0, 0, 24);

let relativeTo = { year: 2000, month: 4, day: 2, hour: 2, minute: 30, timeZone: dstTimeZoneObserver, calendar };
instance.total({ unit: "days", relativeTo });

const expected = [
  // GetTemporalCalendarSlotValueWithISODefault
  "has calendar.dateAdd",
  "has calendar.dateFromFields",
  "has calendar.dateUntil",
  "has calendar.day",
  "has calendar.dayOfWeek",
  "has calendar.dayOfYear",
  "has calendar.daysInMonth",
  "has calendar.daysInWeek",
  "has calendar.daysInYear",
  "has calendar.fields",
  "has calendar.id",
  "has calendar.inLeapYear",
  "has calendar.mergeFields",
  "has calendar.month",
  "has calendar.monthCode",
  "has calendar.monthDayFromFields",
  "has calendar.monthsInYear",
  "has calendar.weekOfYear",
  "has calendar.year",
  "has calendar.yearMonthFromFields",
  "has calendar.yearOfWeek",
  // lookup
  "get calendar.dateFromFields",
  "get calendar.fields",
  // CalendarFields
  "call calendar.fields",
  // InterpretTemporalDateTimeFields
  "call calendar.dateFromFields",
  // ToTemporalTimeZoneSlotValue
  "has timeZone.getOffsetNanosecondsFor",
  "has timeZone.getPossibleInstantsFor",
  "has timeZone.id",
  // lookup
  "get timeZone.getOffsetNanosecondsFor",
  "get timeZone.getPossibleInstantsFor",
  // InterpretISODateTimeOffset
  "call timeZone.getPossibleInstantsFor",
];

const expectedSpringForward = expected.concat([
  // DisambiguatePossibleInstants
  "call timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "call timeZone.getPossibleInstantsFor",
]);
assert.compareArray(
  actual.slice(0, expectedSpringForward.length), // ignore operations after ToRelativeTemporalObject
  expectedSpringForward,
  "order of operations converting property bag at skipped wall-clock time"
);
actual.splice(0); // clear

relativeTo = { year: 2000, month: 10, day: 29, hour: 1, minute: 30, timeZone: dstTimeZoneObserver, calendar };
instance.total({ unit: "days", relativeTo });

assert.compareArray(
  actual.slice(0, expected.length), // ignore operations after ToRelativeTemporalObject
  expected,
  "order of operations converting property bag at repeated wall-clock time"
);
actual.splice(0); // clear
