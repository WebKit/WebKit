// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainyearmonth
description: >
  User-observable time zone and calendar accesses and calls in
  toPlainYearMonth() happen the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get this.calendar.fields",
  "get this.calendar.yearMonthFromFields",
  "get this.timeZone.getOffsetNanosecondsFor",
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.calendar.fields",
  "get this.calendar.monthCode",
  "call this.calendar.monthCode",
  "get this.calendar.year",
  "call this.calendar.year",
  "call this.calendar.yearMonthFromFields",
];
const actual = [];

const timeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone");
const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.ZonedDateTime(0n, timeZone, calendar);
Object.defineProperties(instance, {
  monthCode: {
    get() {
      actual.push("get this.monthCode");
      return TemporalHelpers.toPrimitiveObserver(actual, "M01", "this.monthCode");
    }
  },
  year: {
    get() {
      actual.push("get this.year");
      return TemporalHelpers.toPrimitiveObserver(actual, 1970, "this.year");
    }
  },
});
// clear observable operations that occurred during the constructor call
actual.splice(0);

instance.toPlainYearMonth();
assert.compareArray(actual, expected, "order of operations");
