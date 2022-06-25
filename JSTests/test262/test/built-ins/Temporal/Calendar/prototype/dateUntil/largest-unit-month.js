// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Temporal.Calendar.prototype.dateUntil with largestUnit is "month"
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. Set one to ? ToTemporalDate(one).
  5. Set two to ? ToTemporalDate(two).
  6. Set options to ? GetOptionsObject(options).
  7. Let largestUnit be ? ToLargestTemporalUnit(options, « "hour", "minute", "second", "millisecond", "microsecond", "nanosecond" », "auto", "day").
  8. Let result be ! DifferenceISODate(one.[[ISOYear]], one.[[ISOMonth]], one.[[ISODay]], two.[[ISOYear]], two.[[ISOMonth]], two.[[ISODay]], largestUnit).
  9. Return ? CreateTemporalDuration(result.[[Years]], result.[[Months]], result.[[Weeks]], result.[[Days]], 0, 0, 0, 0, 0, 0).
features: [Temporal]
includes: [temporalHelpers.js]
---*/
let cal = new Temporal.Calendar("iso8601");

["month", "months"].forEach(function(largestUnit) {
  let opt = {largestUnit};
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-07-16", opt),
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "same day");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-07-17", opt),
      0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "one day");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-07-23", opt),
      0, 0, 0, 7, 0, 0, 0, 0, 0, 0, "7 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-08-16", opt),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, "1 month in same year");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2020-12-16", "2021-01-16", opt),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, "1 month in different year");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-01-05", "2021-02-05", opt),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, "1 month in same year");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-01-07", "2021-03-07", opt),
      0, 2, 0, 0, 0, 0, 0, 0, 0, 0, "2 month in same year across Feb 28");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-08-17", opt),
      0, 1, 0, 1, 0, 0, 0, 0, 0, 0, "1 month and 1 day in a month with 31 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-08-13", opt),
      0, 0, 0, 28, 0, 0, 0, 0, 0, 0, "28 days roll across a month which has 31 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-09-16", opt),
      0, 2, 0, 0, 0, 0, 0, 0, 0, 0, "2 months with both months which have 31 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2022-07-16", opt),
      0, 12, 0, 0, 0, 0, 0, 0, 0, 0, "12 months");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2031-07-16", opt),
      0, 120, 0, 0, 0, 0, 0, 0, 0, 0, "120 months");

  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-17", "2021-07-16", opt),
      0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "negative one day");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-23", "2021-07-16", opt),
      0, 0, 0, -7, 0, 0, 0, 0, 0, 0, "negative 7 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-08-16", "2021-07-16", opt),
      0, -1, 0, 0, 0, 0, 0, 0, 0, 0, "negative 1 month in same year");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-01-16", "2020-12-16", opt),
      0, -1, 0, 0, 0, 0, 0, 0, 0, 0, "negative 1 month in different year");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-02-05", "2021-01-05", opt),
      0, -1, 0, 0, 0, 0, 0, 0, 0, 0, "negative 1 month in same year");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-03-07", "2021-01-07", opt),
      0, -2, 0, 0, 0, 0, 0, 0, 0, 0, "negative 2 month in same year across Feb 28");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-08-17", "2021-07-16", opt),
      0, -1, 0, -1, 0, 0, 0, 0, 0, 0, "negative 1 month and 1 day in a month with 31 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-08-13", "2021-07-16", opt),
      0, 0, 0, -28, 0, 0, 0, 0, 0, 0, "negative 28 days roll across a month which has 31 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-09-16", "2021-07-16", opt),
      0, -2, 0, 0, 0, 0, 0, 0, 0, 0, "negative 2 months with both months which have 31 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2022-07-16", "2021-07-16", opt),
      0, -12, 0, 0, 0, 0, 0, 0, 0, 0, "negative 12 months");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2031-07-16", "2021-07-16", opt),
      0, -120, 0, 0, 0, 0, 0, 0, 0, 0, "negative 120 months");
});
