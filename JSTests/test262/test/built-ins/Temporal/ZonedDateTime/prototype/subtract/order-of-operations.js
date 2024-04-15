// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.subtract
description: Properties on objects passed to subtract() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  // ToTemporalDurationRecord
  "get duration.days",
  "get duration.days.valueOf",
  "call duration.days.valueOf",
  "get duration.hours",
  "get duration.hours.valueOf",
  "call duration.hours.valueOf",
  "get duration.microseconds",
  "get duration.microseconds.valueOf",
  "call duration.microseconds.valueOf",
  "get duration.milliseconds",
  "get duration.milliseconds.valueOf",
  "call duration.milliseconds.valueOf",
  "get duration.minutes",
  "get duration.minutes.valueOf",
  "call duration.minutes.valueOf",
  "get duration.months",
  "get duration.months.valueOf",
  "call duration.months.valueOf",
  "get duration.nanoseconds",
  "get duration.nanoseconds.valueOf",
  "call duration.nanoseconds.valueOf",
  "get duration.seconds",
  "get duration.seconds.valueOf",
  "call duration.seconds.valueOf",
  "get duration.weeks",
  "get duration.weeks.valueOf",
  "call duration.weeks.valueOf",
  "get duration.years",
  "get duration.years.valueOf",
  "call duration.years.valueOf",
  // lookup
  "get this.timeZone.getOffsetNanosecondsFor",
  "get this.timeZone.getPossibleInstantsFor",
  "get this.calendar.dateAdd",
  // AddZonedDateTime
  "call this.timeZone.getOffsetNanosecondsFor",
  "call this.calendar.dateAdd",
  // ... inside Calendar.p.dateAdd
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
  // AddZonedDateTime again
  "call this.timeZone.getPossibleInstantsFor",
];
const actual = [];

const timeZone = TemporalHelpers.timeZoneObserver(actual, "this.timeZone");
const calendar = TemporalHelpers.calendarObserver(actual, "this.calendar");
const instance = new Temporal.ZonedDateTime(0n, timeZone, calendar);
// clear observable operations that occurred during the constructor call
actual.splice(0);

const duration = TemporalHelpers.propertyBagObserver(actual, {
  years: 1,
  months: 1,
  weeks: 1,
  days: 1,
  hours: 1,
  minutes: 1,
  seconds: 1,
  milliseconds: 1,
  microseconds: 1,
  nanoseconds: 1,
}, "duration");

const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

instance.subtract(duration, options);
assert.compareArray(actual, expected, "order of operations");
