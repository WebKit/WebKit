// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
  RoundDuration computes in such a way as to avoid precision loss when the
  computed day, week, month, and year lengths are very large numbers.
info: |
  RoundDuration:
    ...
    7. If _unit_ is one of *"year"*, *"month"*, *"week"*, or *"day"*, then
      a. If _zonedRelativeTo_ is not *undefined*, then
        ...
        iii. Let _fractionalDays_ be _days_ + _result_.[[Days]] + DivideNormalizedTimeDuration(_result_.[[Remainder]], _result_.[[DayLength]]).
    ...
    10. If _unit_ is *"year"*, then
      ...
      z. Let _fractionalYears_ be _years_ + _fractionalDays_ / abs(_oneYearDays_).
    ...
    11. If _unit_ is *"month"*, then
      ...
      z. Let _fractionalMonths_ be _months_ + _fractionalDays_ / abs(_oneMonthDays_).
    ...
    12. If _unit_ is *"week"*, then
      ...
      s. Let _fractionalWeeks_ be _weeks_ + _fractionalDays_ / abs(_oneWeekDays_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

// Return the next Number value in direction to +Infinity.
function nextUp(num) {
  if (!Number.isFinite(num)) {
    return num;
  }
  if (num === 0) {
    return Number.MIN_VALUE;
  }

  var f64 = new Float64Array([num]);
  var u64 = new BigUint64Array(f64.buffer);
  u64[0] += (num < 0 ? -1n : 1n);
  return f64[0];
}

// Return the next Number value in direction to -Infinity.
function nextDown(num) {
  if (!Number.isFinite(num)) {
    return num;
  }
  if (num === 0) {
    return -Number.MIN_VALUE;
  }

  var f64 = new Float64Array([num]);
  var u64 = new BigUint64Array(f64.buffer);
  u64[0] += (num < 0 ? 1n : -1n);
  return f64[0];
}

// Return bit pattern representation of Number as a Uint8Array of bytes.
function f64Repr(f) {
  const buf = new ArrayBuffer(8);
  new DataView(buf).setFloat64(0, f);
  return new Uint8Array(buf);
}

// ============
// Set up contrived custom time zone to give the denominator of the fraction its
// largest possible value

function createTimeZone() {
  const tz = new Temporal.TimeZone("UTC");
  TemporalHelpers.substituteMethod(tz, "getPossibleInstantsFor", [
    TemporalHelpers.SUBSTITUTE_SKIP,  // Duration.total step 18.c AddZonedDateTime
    TemporalHelpers.SUBSTITUTE_SKIP,  // DifferenceZonedDateTimeWithRounding → DifferenceZonedDateTime → AddZonedDateTime
    [new Temporal.Instant(-86400_0000_0000_000_000_000n)],
    [new Temporal.Instant(86400_0000_0000_000_000_000n)],
  ]);
  return tz;
}

function createRelativeTo() {
  return new Temporal.ZonedDateTime(-86400_0000_0000_000_000_000n, createTimeZone());
}

// ============

// We will calculate the total years of a duration of 1 year, 0.009254648 s

// NS_MAX_INSTANT = 86400 × 1e8 × 1e9
// startEpochNs = -NS_MAX_INSTANT
// destEpochNs = -NS_MAX_INSTANT + 366 * 86400 * 1e9 + 9254648
// endEpochNs = NS_MAX_INSTANT
// progress = (destEpochNs - startEpochNs) / (endEpochNs - startEpochNs)
//   = (366 * 86400 * 1e9 + 9254648) / (2 × NS_MAX_INSTANT)
// total = startDuration.years + progress
//   = 1 + (366 * 86400 * 1e9 + 9254648) / (2 × NS_MAX_INSTANT)
//
// Calculated with Python's Decimal module to 50 decimal places
const expected = 1.000001830000000_5355699074074074074074074074074074;
// This is float 1.000001830000000_64659
// Note: if calculated using floating point arithmetic, this will give the less
// precise value 1.000001830000000_42454

// Check that we are not accidentally losing precision in our expected value:

assert.sameValue(expected, 1.000001830000000_64659, "the float representation of the result is 1.00000183000000064659");
assert.compareArray(
  f64Repr(expected),
  [0x3f, 0xf0, 0x00, 0x01, 0xeb, 0x3c, 0xa4, 0x79],
  "the bit representation of the result is 0x3ff00001eb3ca479"
);
// The next Number in direction -Infinity is less precise.
assert.sameValue(nextDown(expected), 1.000001830000000_42455, "the next Number in direction -Infinity is less precise");
// The next Number in direction +Infinity is less precise.
assert.sameValue(nextUp(expected), 1.000001830000000_86864, "the next Number in direction +Infinity is less precise");

// ============

const duration = new Temporal.Duration(/* years = */ 1, 0, 0, 0, 0, 0, 0, 9, 254, 648);
assert.sameValue(duration.total({ unit: "years", relativeTo: createRelativeTo() }), expected, "Correct division by large number in years total");
