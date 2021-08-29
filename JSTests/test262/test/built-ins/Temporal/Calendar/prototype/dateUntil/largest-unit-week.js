// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Temporal.Calendar.prototype.dateUntil with largestUnit is "week"
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

["week", "weeks"].forEach(function(largestUnit) {
  let opt = {largestUnit};

  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-07-16", opt),
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "same day");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-07-17", opt),
      0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "one day");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-07-23", opt),
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0, "7 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-08-16", opt),
      0, 0, 4, 3, 0, 0, 0, 0, 0, 0, "4 weeks and 3 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-08-13", opt),
      0, 0, 4, 0, 0, 0, 0, 0, 0, 0, "4 weeks");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2021-09-16", opt),
      0, 0, 8, 6, 0, 0, 0, 0, 0, 0, "8 weeks and 6 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2022-07-16", opt),
      0, 0, 52, 1, 0, 0, 0, 0, 0, 0, "52 weeks and 1 day");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-16", "2031-07-16", opt),
      0, 0, 521, 5, 0, 0, 0, 0, 0, 0, "521 weeks and 5 days");

  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-17", "2021-07-16", opt),
      0, 0, 0, -1, 0, 0, 0, 0, 0, 0, "negative one day");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-07-23", "2021-07-16", opt),
      0, 0, -1, 0, 0, 0, 0, 0, 0, 0, "negative 7 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-08-16", "2021-07-16", opt),
      0, 0, -4, -3, 0, 0, 0, 0, 0, 0, "negative 4 weeks and 3 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-08-13", "2021-07-16", opt),
      0, 0, -4, 0, 0, 0, 0, 0, 0, 0, "negative 4 weeks");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2021-09-16", "2021-07-16", opt),
      0, 0, -8, -6, 0, 0, 0, 0, 0, 0, "negative 8 weeks and 6 days");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2022-07-16", "2021-07-16", opt),
      0, 0, -52, -1, 0, 0, 0, 0, 0, 0, "negative 52 weeks and 1 day");
  TemporalHelpers.assertDuration(
      cal.dateUntil("2031-07-16", "2021-07-16", opt),
      0, 0, -521, -5, 0, 0, 0, 0, 0, 0, "negative 521 weeks and 5 days");
});
