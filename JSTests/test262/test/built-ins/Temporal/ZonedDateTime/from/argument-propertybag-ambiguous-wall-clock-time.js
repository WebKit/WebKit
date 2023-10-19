// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: >
  Correct time zone calls are made when converting a ZonedDateTime-like property
  bag denoting an ambiguous wall-clock time
includes: [temporalHelpers.js, compareArray.js]
features: [Temporal]
---*/

const actual = [];

const dstTimeZone = TemporalHelpers.springForwardFallBackTimeZone();
const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  getOffsetNanosecondsFor: dstTimeZone.getOffsetNanosecondsFor.bind(dstTimeZone),
  getPossibleInstantsFor: dstTimeZone.getPossibleInstantsFor.bind(dstTimeZone),
});
const calendar = TemporalHelpers.calendarObserver(actual, "calendar");

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
  // CalendarFields
  "get calendar.fields",
  "call calendar.fields",
  // ToTemporalTimeZoneSlotValue
  "has timeZone.getOffsetNanosecondsFor",
  "has timeZone.getPossibleInstantsFor",
  "has timeZone.id",
  // InterpretTemporalDateTimeFields
  "get calendar.dateFromFields",
  "call calendar.dateFromFields",
];

Temporal.ZonedDateTime.from(
  { year: 2000, month: 4, day: 2, hour: 2, minute: 30, offset: "-08:00", timeZone, calendar },
  { offset: "use" }
);
assert.compareArray(actual, expected, "order of operations converting property bag at skipped wall-clock time with offset: use");
actual.splice(0); // clear

Temporal.ZonedDateTime.from(
  { year: 2000, month: 4, day: 2, hour: 2, minute: 30, offset: "-08:00", timeZone, calendar },
  { offset: "ignore" }
);
assert.compareArray(actual, expected.concat([
  // InterpretISODateTimeOffset
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
  // DisambiguatePossibleInstants
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
]), "order of operations converting property bag at skipped wall-clock time with offset: ignore");
actual.splice(0); // clear

Temporal.ZonedDateTime.from(
  { year: 2000, month: 4, day: 2, hour: 2, minute: 30, offset: "-08:00", timeZone, calendar },
  { offset: "prefer" }
);
assert.compareArray(actual, expected.concat([
  // InterpretISODateTimeOffset
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
  // DisambiguatePossibleInstants
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
]), "order of operations converting property bag at skipped wall-clock time with offset: prefer");
actual.splice(0); // clear

Temporal.ZonedDateTime.from(
  { year: 2000, month: 10, day: 29, hour: 1, minute: 30, offset: "-08:00", timeZone, calendar },
  { offset: "use" }
);
assert.compareArray(actual, expected, "order of operations converting property bag at repeated wall-clock time with offset: use");
actual.splice(0); // clear

Temporal.ZonedDateTime.from(
  { year: 2000, month: 10, day: 29, hour: 1, minute: 30, offset: "-08:00", timeZone, calendar },
  { offset: "ignore" }
);
assert.compareArray(actual, expected.concat([
  // InterpretISODateTimeOffset
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
]), "order of operations converting property bag at repeated wall-clock time with offset: ignore");
actual.splice(0); // clear

Temporal.ZonedDateTime.from(
  { year: 2000, month: 10, day: 29, hour: 1, minute: 30, offset: "-08:00", timeZone, calendar },
  { offset: "prefer" }
);
assert.compareArray(actual, expected.concat([
  // InterpretISODateTimeOffset
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
]), "order of operations converting property bag at repeated wall-clock time with offset: prefer");
actual.splice(0); // clear

Temporal.ZonedDateTime.from(
  { year: 2000, month: 10, day: 29, hour: 1, minute: 30, offset: "-08:00", timeZone, calendar },
  { offset: "reject" }
);
assert.compareArray(actual, expected.concat([
  // InterpretISODateTimeOffset
  "get timeZone.getPossibleInstantsFor",
  "call timeZone.getPossibleInstantsFor",
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
]), "order of operations converting property bag at repeated wall-clock time with offset: reject");
actual.splice(0); // clear
