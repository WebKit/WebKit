// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  No calendar or time zone methods are called under circumstances where rounding
  is a no-op
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarThrowEverything();
const timeZone = TemporalHelpers.timeZoneThrowEverything();
const plainRelativeTo = new Temporal.PlainDate(2000, 1, 1, calendar);
const zonedRelativeTo = new Temporal.ZonedDateTime(0n, timeZone, calendar);

const d = new Temporal.Duration(0, 0, 0, 0, 23, 59, 59, 999, 999, 997);

const noopRoundingOperations = [
  [d, { smallestUnit: "nanoseconds" }, "smallestUnit ns"],
  [d, { smallestUnit: "nanoseconds", relativeTo: plainRelativeTo }, "smallestUnit ns and plain relativeTo"],
  [d, { smallestUnit: "nanoseconds", relativeTo: zonedRelativeTo }, "smallestUnit ns and zoned relativeTo"],
  [d, { smallestUnit: "nanoseconds", roundingIncrement: 1 }, "round to 1 ns"],
  // No balancing because largestUnit is already the largest unit and no time units overflow:
  [d, { largestUnit: "hours" }, "largestUnit hours"],
  // Unless relativeTo is ZonedDateTime, no-op is still possible with days>0:
  [new Temporal.Duration(0, 0, 0, 1), { smallestUnit: "nanoseconds" }, "days>0 and smallestUnit ns"],
  [new Temporal.Duration(0, 0, 0, 1), { smallestUnit: "nanoseconds", relativeTo: plainRelativeTo }, "days>0, smallestUnit ns, and plain relativeTo"],
];
for (const [duration, options, descr] of noopRoundingOperations) {
  const result = duration.round(options);
  assert.notSameValue(result, duration, "rounding result should be a new object");
  TemporalHelpers.assertDurationsEqual(result, duration, `rounding should be a no-op with ${descr}`);

  const negDuration = duration.negated();
  const negResult = negDuration.round(options);
  assert.notSameValue(negResult, negDuration, "rounding result should be a new object (negative)");
  TemporalHelpers.assertDurationsEqual(negResult, negDuration, `rounding should be a no-op with ${descr} (negative)`);
}

// These operations are not no-op rounding operations, but still should not call
// any calendar methods:
const roundingOperationsNotCallingCalendarMethods = [
  [d, { smallestUnit: "microseconds" }, "round to 1 µs"],
  [d, { smallestUnit: "nanoseconds", roundingIncrement: 2 }, "round to 2 ns"],
  [new Temporal.Duration(0, 0, 0, 0, 24), { largestUnit: "days" }, "upwards balancing requested"],
  [d, { largestUnit: "minutes" }, "downwards balancing requested"],
  [new Temporal.Duration(0, 0, 0, 0, 1, 120), { smallestUnit: "nanoseconds" }, "time units could overflow"],
  [new Temporal.Duration(0, 0, 0, 1, 24), { smallestUnit: "nanoseconds" }, "hours-to-days conversion could occur"],
];
for (const [duration, options, descr] of roundingOperationsNotCallingCalendarMethods) {
  const result = duration.round(options);
  let equal = true;
  for (const prop of ['years', 'months', 'weeks', 'days', 'hours', 'minutes', 'seconds', 'milliseconds', 'microseconds', 'nanoseconds']) {
    if (result[prop] !== duration[prop]) {
      equal = false;
      break;
    }
  }
  assert(!equal, `round result ${result} should be different from ${duration} with ${descr}`);

  const negDuration = duration.negated();
  const negResult = negDuration.round(options);
  equal = true;
  for (const prop of ['years', 'months', 'weeks', 'days', 'hours', 'minutes', 'seconds', 'milliseconds', 'microseconds', 'nanoseconds']) {
    if (negResult[prop] !== negDuration[prop]) {
      equal = false;
      break;
    }
  }
  assert(!equal, `round result ${negResult} should be different from ${negDuration} with ${descr} (negative)`);
}

// These operations should not be short-circuited because they have to call
// calendar methods:
const roundingOperationsCallingCalendarMethods = [
  [new Temporal.Duration(0, 0, 1), { smallestUnit: "nanoseconds", relativeTo: plainRelativeTo }, "calendar units present"],
  [d, { largestUnit: "days", relativeTo: zonedRelativeTo }, "largestUnit days with zoned relativeTo"],
  [new Temporal.Duration(0, 0, 0, 1), { smallestUnit: "nanoseconds", relativeTo: zonedRelativeTo }, "hours-to-days conversion could occur with zoned relativeTo"],
];

for (const [duration, options, descr] of roundingOperationsCallingCalendarMethods) {
  assert.throws(Test262Error, () => duration.round(options), `rounding should not be a no-op with ${descr}`);
  assert.throws(Test262Error, () => duration.negated().round(options), `rounding should not be a no-op with ${descr} (negative)`);
}
