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

function createTimeZone() {
  const tz = new Temporal.TimeZone("UTC");
  TemporalHelpers.substituteMethod(tz, "getPossibleInstantsFor", [
    TemporalHelpers.SUBSTITUTE_SKIP,
    [new Temporal.Instant(-86400_0000_0000_000_000_000n)],
    [new Temporal.Instant(86400_0000_0000_000_000_000n - 100_000_000n)],
  ]);
  return tz;
}

function createRelativeTo() {
  return new Temporal.ZonedDateTime(-86400_0000_0000_000_000_000n, createTimeZone());
}

const d = new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, /* nanoseconds = */ 1);

// NS_MAX_INSTANT = 86400 × 1e8 × 1e9
// startEpochNs = -NS_MAX_INSTANT
// destEpochNs = -NS_MAX_INSTANT + 1
// endEpochNs = NS_MAX_INSTANT - 1e8
// progress = (destEpochNs - startEpochNs) / (endEpochNs - startEpochNs)
//   = 1 / (2 × NS_MAX_INSTANT - 1e8)
// total = startDuration.years + progress = 0 + 1 / (2 × NS_MAX_INSTANT - 1e8)
//
// Calculated with Python's Decimal module to 50 decimal places
const expected = 5.7870370370370_705268347050756396228860247432848547e-23;
// This is float 5.7870370370370_6998177e-23
// Note: if calculated using floating point arithmetic, this will give the less
// precise value 5.7870370370370_7115726e-23

// Check that we are not accidentally losing precision in our expected value:
assert.sameValue(expected, 5.7870370370370_6998177e-23, "the float representation of the result is 5.7870370370370_6998177e-23");
assert.compareArray(
  f64Repr(expected),
  [0x3b, 0x51, 0x7d, 0x80, 0xc6, 0xf1, 0x14, 0xa8],
  "the bit representation of the result is 0x3b517d80c6f114a8"
);
// The next Number in direction -Infinity is less precise.
assert.sameValue(nextDown(expected), 5.7870370370370_6880628e-23, "the next Number in direction -Infinity is less precise");
// The next Number in direction +Infinity is less precise.
assert.sameValue(nextUp(expected), 5.7870370370370_7115727e-23, "the next Number in direction +Infinity is less precise");

assert.sameValue(d.total({ unit: "years", relativeTo: createRelativeTo() }), expected, "Correct division by large number in years total");
assert.sameValue(d.total({ unit: "months", relativeTo: createRelativeTo() }), expected, "Correct division by large number in months total");
assert.sameValue(d.total({ unit: "weeks", relativeTo: createRelativeTo() }), expected, "Correct division by large number in weeks total");
