// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
description: Observable interactions with the provided calendar-like object
includes: [compareArray.js]
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
  'get nestedCalendar.Symbol(Symbol.toPrimitive)',
  'get nestedCalendar.toString',
  'call nestedCalendar.toString'
];
const nestedCalendar = new Proxy({
  toString: function() {
    actual.push('call nestedCalendar.toString');
    return 'iso8601';
  }
}, {
  has(target, property) {
    actual.push(`has nestedCalendar.${String(property)}`);
    return property in target;
  },
  get(target, property) {
    actual.push(`get nestedCalendar.${String(property)}`);
    return target[property];
  },
});
const calendar = new Proxy({
  calendar: nestedCalendar,
  toString: function() {
    actual.push('call calendar.toString');
    return 'iso8601';
  },
}, {
  has(target, property) {
    actual.push(`has calendar.${String(property)}`);
    return property in target;
  },
  get(target, property) {
    actual.push(`get calendar.${String(property)}`);
    return target[property];
  },
});

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
