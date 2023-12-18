// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: User code calls happen in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "get item.plainDate",
  "get item.plainDate.calendar",
  "has item.plainDate.calendar.dateAdd",
  "has item.plainDate.calendar.dateFromFields",
  "has item.plainDate.calendar.dateUntil",
  "has item.plainDate.calendar.day",
  "has item.plainDate.calendar.dayOfWeek",
  "has item.plainDate.calendar.dayOfYear",
  "has item.plainDate.calendar.daysInMonth",
  "has item.plainDate.calendar.daysInWeek",
  "has item.plainDate.calendar.daysInYear",
  "has item.plainDate.calendar.fields",
  "has item.plainDate.calendar.id",
  "has item.plainDate.calendar.inLeapYear",
  "has item.plainDate.calendar.mergeFields",
  "has item.plainDate.calendar.month",
  "has item.plainDate.calendar.monthCode",
  "has item.plainDate.calendar.monthDayFromFields",
  "has item.plainDate.calendar.monthsInYear",
  "has item.plainDate.calendar.weekOfYear",
  "has item.plainDate.calendar.year",
  "has item.plainDate.calendar.yearMonthFromFields",
  "has item.plainDate.calendar.yearOfWeek",
  "get item.plainDate.calendar.fields",
  "call item.plainDate.calendar.fields",
  "get item.plainDate.day",
  "get item.plainDate.day.valueOf",
  "call item.plainDate.day.valueOf",
  "get item.plainDate.month",
  "get item.plainDate.month.valueOf",
  "call item.plainDate.month.valueOf",
  "get item.plainDate.monthCode",
  "get item.plainDate.monthCode.toString",
  "call item.plainDate.monthCode.toString",
  "get item.plainDate.year",
  "get item.plainDate.year.valueOf",
  "call item.plainDate.year.valueOf",
  "get item.plainDate.calendar.dateFromFields",
  "call item.plainDate.calendar.dateFromFields",
  "get item.timeZone",
  "has item.timeZone.getOffsetNanosecondsFor",
  "has item.timeZone.getPossibleInstantsFor",
  "has item.timeZone.id",
  "get item.timeZone.getPossibleInstantsFor",
  "call item.timeZone.getPossibleInstantsFor",
];

const calendar = TemporalHelpers.calendarObserver(actual, "item.plainDate.calendar");
const instance = new Temporal.PlainTime(2, 30);

const dstTimeZone = TemporalHelpers.springForwardFallBackTimeZone();
const timeZone = TemporalHelpers.timeZoneObserver(actual, "item.timeZone", {
  getOffsetNanosecondsFor: dstTimeZone.getOffsetNanosecondsFor,
  getPossibleInstantsFor: dstTimeZone.getPossibleInstantsFor,
});

const plainDate = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 1,
  monthCode: "M01",
  day: 1,
  calendar,
}, "item.plainDate");
instance.toZonedDateTime(TemporalHelpers.propertyBagObserver(actual, {
  plainDate,
  timeZone,
}, "item"));
assert.compareArray(actual, expected, "order of operations at normal wall-clock time");
actual.splice(0); // clear

const fallBackPlainDate = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 10,
  monthCode: "M10",
  day: 29,
  calendar,
}, "item.plainDate");
const fallBackInstance = new Temporal.PlainTime(1, 30);
fallBackInstance.toZonedDateTime(TemporalHelpers.propertyBagObserver(actual, {
  plainDate: fallBackPlainDate,
  timeZone,
}, "item"));
assert.compareArray(actual, expected, "order of operations at repeated wall-clock time");
actual.splice(0); // clear

const springForwardPlainDate = TemporalHelpers.propertyBagObserver(actual, {
  year: 2000,
  month: 4,
  monthCode: "M04",
  day: 2,
  calendar,
}, "item.plainDate");
instance.toZonedDateTime(TemporalHelpers.propertyBagObserver(actual, {
  plainDate: springForwardPlainDate,
  timeZone,
}, "item"));
assert.compareArray(actual, expected.concat([
  "get item.timeZone.getOffsetNanosecondsFor",
  "call item.timeZone.getOffsetNanosecondsFor",
  "call item.timeZone.getOffsetNanosecondsFor",
  "get item.timeZone.getPossibleInstantsFor",
  "call item.timeZone.getPossibleInstantsFor",
]), "order of operations at skipped wall-clock time");
actual.splice(0); // clear
