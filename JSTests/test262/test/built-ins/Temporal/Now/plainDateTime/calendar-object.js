// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
description: Observable interactions with the provided calendar-like object
includes: [compareArray.js, temporalHelpers.js]
features: [Proxy, Temporal]
---*/

const actual = [];
const expectedWithout = [
  'has calendar.calendar',
  'get calendar.calendar',
  'has nestedCalendar.calendar'
];
const expectedWith = [
  'has calendar.calendar',
  'get calendar.calendar',
  'has nestedCalendar.calendar',
  'get nestedCalendar[Symbol.toPrimitive]',
  'get nestedCalendar.toString',
  'call nestedCalendar.toString'
];
const nestedCalendar = TemporalHelpers.calendarObserver(actual, "nestedCalendar", {
  toString: "iso8601",
});
const calendar = TemporalHelpers.calendarObserver(actual, "calendar", {
  toString: "iso8601",
});
calendar.calendar = nestedCalendar;

Object.defineProperty(Temporal.Calendar, 'from', {
  get() {
    actual.push('get Temporal.Calendar.from');
    return undefined;
  },
});

Temporal.Now.plainDateTime(calendar);

assert.compareArray(actual, expectedWithout, 'Observable interactions without `calendar` property');

actual.length = 0;
nestedCalendar.calendar = null;

Temporal.Now.plainDateTime(calendar);

assert.compareArray(actual, expectedWith, 'Observable interactions with `calendar` property');
