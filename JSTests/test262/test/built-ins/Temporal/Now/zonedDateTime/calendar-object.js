// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.zoneddatetime
description: Observable interactions with the provided calendar-like object
includes: [compareArray.js, temporalHelpers.js]
features: [Proxy, Temporal]
---*/

const actual = [];
const expected = [
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
];

const calendar = TemporalHelpers.calendarObserver(actual, "calendar", {
  toString: "iso8601",
});

Object.defineProperty(Temporal.Calendar, 'from', {
  get() {
    actual.push('get Temporal.Calendar.from');
    return undefined;
  },
});

Temporal.Now.zonedDateTime(calendar);

assert.compareArray(actual, expected, 'order of observable operations');
