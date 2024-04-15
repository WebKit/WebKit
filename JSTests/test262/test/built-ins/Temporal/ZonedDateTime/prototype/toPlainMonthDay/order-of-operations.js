// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainmonthday
description: >
  User-observable time zone and calendar accesses and calls in
  toPlainMonthDay() happen the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get this.calendar.fields",
  "get this.calendar.monthDayFromFields",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.calendar.fields",
  "get this.calendar.day",
  "call this.calendar.day",
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "call this.calendar.monthDayFromFields",
];
const actual = [];

const timeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone");
const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.ZonedDateTime(0n, timeZone, calendar);
Object.defineProperties(instance, {
  day: {
    get() {
      actual.push("get this.day");
      return TemporalHelpers.toPrimitiveObserver(actual, 1, "this.day");
    }
  },
  monthCode: {
    get() {
      actual.push("get this.monthCode");
      return TemporalHelpers.toPrimitiveObserver(actual, "M01", "this.monthCode");
    }
  },
});
// clear observable operations that occurred during the constructor call
actual.splice(0);

instance.toPlainMonthDay();
assert.compareArray(actual, expected, "order of operations");
