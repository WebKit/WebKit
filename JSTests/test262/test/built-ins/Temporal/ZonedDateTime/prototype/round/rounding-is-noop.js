// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: >
  No calendar or time zone methods are called under circumstances where rounding
  is a no-op
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarThrowEverything();
const timeZone = TemporalHelpers.timeZoneThrowEverything();
const instance = new Temporal.ZonedDateTime(0n, timeZone, calendar);

const noopRoundingOperations = [
  [{ smallestUnit: "nanoseconds" }, "smallestUnit ns"],
  [{ smallestUnit: "nanoseconds", roundingIncrement: 1 }, "round to 1 ns"],
];
for (const [options, descr] of noopRoundingOperations) {
  const result = instance.round(options);
  assert.notSameValue(result, instance, "rounding result should be a new object");
  assert.sameValue(result.epochNanoseconds, instance.epochNanoseconds, "instant should be unchanged");
  assert.sameValue(result.getCalendar(), instance.getCalendar(), "calendar should be preserved");
  assert.sameValue(result.getTimeZone(), instance.getTimeZone(), "time zone should be preserved");
}

const notNoopRoundingOperations = [
  [{ smallestUnit: "microseconds" }, "round to 1 µs"],
  [{ smallestUnit: "nanoseconds", roundingIncrement: 2 }, "round to 2 ns"],
];
for (const [options, descr] of notNoopRoundingOperations) {
  assert.throws(Test262Error, () => instance.round(options), `rounding should not be a no-op with ${descr}`);
}
