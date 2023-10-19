// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.withplaindate
description: User code calls happen in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  // ToTemporalDate
  "get plainDateLike.calendar",
  "has plainDateLike.calendar.dateAdd",
  "has plainDateLike.calendar.dateFromFields",
  "has plainDateLike.calendar.dateUntil",
  "has plainDateLike.calendar.day",
  "has plainDateLike.calendar.dayOfWeek",
  "has plainDateLike.calendar.dayOfYear",
  "has plainDateLike.calendar.daysInMonth",
  "has plainDateLike.calendar.daysInWeek",
  "has plainDateLike.calendar.daysInYear",
  "has plainDateLike.calendar.fields",
  "has plainDateLike.calendar.id",
  "has plainDateLike.calendar.inLeapYear",
  "has plainDateLike.calendar.mergeFields",
  "has plainDateLike.calendar.month",
  "has plainDateLike.calendar.monthCode",
  "has plainDateLike.calendar.monthDayFromFields",
  "has plainDateLike.calendar.monthsInYear",
  "has plainDateLike.calendar.weekOfYear",
  "has plainDateLike.calendar.year",
  "has plainDateLike.calendar.yearMonthFromFields",
  "has plainDateLike.calendar.yearOfWeek",
  "get plainDateLike.calendar.fields",
  "call plainDateLike.calendar.fields",
  "get plainDateLike.day",
  "get plainDateLike.day.valueOf",
  "call plainDateLike.day.valueOf",
  "get plainDateLike.month",
  "get plainDateLike.month.valueOf",
  "call plainDateLike.month.valueOf",
  "get plainDateLike.monthCode",
  "get plainDateLike.monthCode.toString",
  "call plainDateLike.monthCode.toString",
  "get plainDateLike.year",
  "get plainDateLike.year.valueOf",
  "call plainDateLike.year.valueOf",
  "get plainDateLike.calendar.dateFromFields",
  "call plainDateLike.calendar.dateFromFields",
  // GetPlainDateTimeFor
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  // ConsolidateCalendars
  "get this.calendar.id",
  "get plainDateLike.calendar.id",
  // GetInstantFor
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
];

const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const plainDateCalendar = TemporalHelpers.calendarObserver(actual, "plainDateLike.calendar");
const dstTimeZone = TemporalHelpers.springForwardFallBackTimeZone();
const timeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone", {
  getOffsetNanosecondsFor: dstTimeZone.getOffsetNanosecondsFor,
  getPossibleInstantsFor: dstTimeZone.getPossibleInstantsFor,
});

const instance = new Temporal.ZonedDateTime(37800_000_000_000n /* 1970-01-01T02:30-08:00 */, timeZone, calendar);
const fallBackInstance = new Temporal.ZonedDateTime(34200_000_000_000n /* 1970-01-01T01:30-08:00 */, timeZone, calendar);
actual.splice(0); // clear calls that happened in constructor

const plainDateLike = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 1,
  monthCode: "M01",
  day: 1,
  calendar: plainDateCalendar,
}, "plainDateLike");
instance.withPlainDate(plainDateLike);
assert.compareArray(actual, expected, "order of operations at normal wall-clock time");
actual.splice(0); // clear

const fallBackPlainDateLike = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 10,
  monthCode: "M10",
  day: 29,
  calendar: plainDateCalendar,
}, "plainDateLike");
fallBackInstance.withPlainDate(fallBackPlainDateLike);
assert.compareArray(actual, expected, "order of operations at repeated wall-clock time");
actual.splice(0); // clear

const springForwardPlainDateLike = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 4,
  monthCode: "M04",
  day: 2,
  calendar: plainDateCalendar,
}, "plainDateLike");
instance.withPlainDate(springForwardPlainDateLike);
assert.compareArray(actual, expected.concat([
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getPossibleInstantsFor",
  "call this.timeZone.getPossibleInstantsFor",
]), "order of operations at skipped wall-clock time");
actual.splice(0); // clear
