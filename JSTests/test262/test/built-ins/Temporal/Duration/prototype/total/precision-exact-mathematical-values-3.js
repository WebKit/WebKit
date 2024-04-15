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
includes: [compareArray.js]
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

const tz = new (class extends Temporal.TimeZone {
  getPossibleInstantsFor() {
    // Called in NormalizedTimeDurationToDays 19 from RoundDuration 7.b.
    // Sets _result_.[[DayLength]] to 2⁵³ - 1 ns, its largest possible value
    return [new Temporal.Instant(-86400_0000_0000_000_000_000n + 2n ** 53n - 1n)];
  }
})("UTC");

const cal = new (class extends Temporal.Calendar {
  dateAdd() {
    // Called in MoveRelativeDate from RoundDuration 10.x, 11.x, or 12.q.
    // Sets _oneYearDays_, _oneMonthDays_, or _oneWeekDays_ respectively to
    // 200_000_000, its largest possible value.
    return new Temporal.PlainDate(275760, 9, 13);
  }
})("iso8601");

const relativeTo = new Temporal.ZonedDateTime(-86400_0000_0000_000_000_000n, tz, cal);
const d = new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, /* nanoseconds = */ 1);

/*
 * RoundDuration step 7:
 *   ii. result = { [[Days]] = 0, [[Remainder]] = normalized time duration of 1 ns,
 *     [[DayLength]] = Number.MAX_SAFE_INTEGER }
 *   iii. fractionalDays = 0 + 0 + 1 / Number.MAX_SAFE_INTEGER
 * step 10:
 *   y. oneYearDays = 200_000_000
 *   z. fractionalYears = 0 + (1 / Number.MAX_SAFE_INTEGER) / 200_000_000
 */
// Calculated with Python's Decimal module to 50 decimal places
const expected = 5.55111512312578_3318415740544369642963189519987393e-25;

// Check that we are not accidentally losing precision in our expected value:

assert.sameValue(expected, 5.55111512312578_373662e-25, "the float representation of the result is 5.55111512312578373662e-25");
assert.compareArray(
  f64Repr(expected),
  [0x3a, 0xe5, 0x79, 0x8e, 0xe2, 0x30, 0x8c, 0x3b],
  "the bit representation of the result is 0x3ae5798ee2308c3b"
);
// The next Number in direction -Infinity is less precise.
assert.sameValue(nextDown(expected), 5.55111512312578_281826e-25, "the next Number in direction -Infinity is less precise");
// The next Number in direction +Infinity is less precise.
assert.sameValue(nextUp(expected), 5.55111512312578_465497e-25, "the next Number in direction +Infinity is less precise");

assert.sameValue(d.total({ unit: "years", relativeTo }), expected, "Correct division by large number in years total");
assert.sameValue(d.total({ unit: "months", relativeTo }), expected, "Correct division by large number in months total");
assert.sameValue(d.total({ unit: "weeks", relativeTo }), expected, "Correct division by large number in weeks total");
