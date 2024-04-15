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
// Set up contrived custom time zone and calendar to give dayLengthNs,
// oneYearDays, oneMonthDays, and oneWeekDays their largest possible values

function createTimeZone() {
  const tz = new Temporal.TimeZone("UTC");
  TemporalHelpers.substituteMethod(tz, "getPossibleInstantsFor", [
    TemporalHelpers.SUBSTITUTE_SKIP,  // Duration.total step 19.a MoveRelativeZonedDateTime → AddZonedDateTime
    TemporalHelpers.SUBSTITUTE_SKIP,  // Duration.total step 19.e.ii AddDaysToZonedDateTime
    TemporalHelpers.SUBSTITUTE_SKIP,  // Duration.total step 19.i.ii NormalizedTimeDurationToDays step 16
    TemporalHelpers.SUBSTITUTE_SKIP,  // Duration.total step 19.i.ii NormalizedTimeDurationToDays step 19
    [new Temporal.Instant(-86400_0000_0000_000_000_000n)],  // RoundDuration step 7.a.i MoveRelativeZonedDateTime → AddZonedDateTime
    [new Temporal.Instant(-86400_0000_0000_000_000_000n + 2n ** 53n - 1n)],  // RoundDuration step 7.a.ii NormalizedTimeDurationToDays step 19
    // sets dayLengthNs to Number.MAX_SAFE_INTEGER
  ]);
  return tz;
}

function createCalendar() {
  const cal = new Temporal.Calendar("iso8601");
  TemporalHelpers.substituteMethod(cal, "dateAdd", [
    TemporalHelpers.SUBSTITUTE_SKIP,  // Duration.total step 19.a MoveRelativeZonedDateTime → AddZonedDateTime
    TemporalHelpers.SUBSTITUTE_SKIP,  // RoundDuration step 7.a.i MoveRelativeZonedDateTime → AddZonedDateTime
    new Temporal.PlainDate(-271821, 4, 20),  // RoundDuration step 10.d/11.d AddDate
    new Temporal.PlainDate(-271821, 4, 20),  // RoundDuration step 10.f/11.f AddDate
    new Temporal.PlainDate(275760, 9, 13),  // RoundDuration step 10.r/11.r MoveRelativeDate
    // sets one{Year,Month,Week}Days to 200_000_000
  ]);
  return cal;
}

// ============

// We will calculate the total years/months/weeks of durations with:
//   1 year/month/week, 1 day, 0.199000001 s

// RoundDuration step 7:
//   ii. result = { [[Days]] = 1, [[Remainder]] = normalized time duration of 199_000_001 ns,
//     [[DayLength]] = Number.MAX_SAFE_INTEGER }
//   iii. fractionalDays = 1 + 0 + 199_000_001 / Number.MAX_SAFE_INTEGER
// step 10: (similar for steps 11 and 12 in the case of months/weeks)
//   y. oneYearDays = 200_000_000
//   z. fractionalYears = 1 + (1 + 199_000_001 / Number.MAX_SAFE_INTEGER) / 200_000_000
//
// Note: if calculated as 1 + 1/200_000_000 + 199_000_001 / Number.MAX_SAFE_INTEGER / 200_000_000
// this will lose precision and give the result 1.000000005.

// Calculated with Python's Decimal module to 50 decimal places
const expected = 1.000000005000000_1104671915053146003490515686745299;

// Check that we are not accidentally losing precision in our expected value:

assert.sameValue(expected, 1.000000005000000_2, "the float representation of the result is 1.0000000050000002");
assert.compareArray(
  f64Repr(expected),
  [0x3f, 0xf0, 0x00, 0x00, 0x01, 0x57, 0x98, 0xef],
  "the bit representation of the result is 0x3ff00000015798ef"
);
// The next Number in direction -Infinity is less precise.
assert.sameValue(nextDown(expected), 1.000000004999999_96961, "the next Number in direction -Infinity is less precise");
// The next Number in direction +Infinity is less precise.
assert.sameValue(nextUp(expected), 1.000000005000000_4137, "the next Number in direction +Infinity is less precise");

// ============

let relativeTo = new Temporal.ZonedDateTime(-86400_0000_0000_000_000_000n, createTimeZone(), createCalendar());
const dYears = new Temporal.Duration(/* years = */ 1, 0, 0, /* days = */ 1, 0, 0, 0, /* milliseconds = */ 199, 0, /* nanoseconds = */ 1);
assert.sameValue(dYears.total({ unit: "years", relativeTo }), expected, "Correct division by large number in years total");

relativeTo = new Temporal.ZonedDateTime(-86400_0000_0000_000_000_000n, createTimeZone(), createCalendar());
const dMonths = new Temporal.Duration(0, /* months = */ 1, 0, /* days = */ 1, 0, 0, 0, /* milliseconds = */ 199, 0, /* nanoseconds = */ 1);
assert.sameValue(dMonths.total({ unit: "months", relativeTo }), expected, "Correct division by large number in months total");

// Weeks calculation doesn't have the AddDate calls to convert months/weeks to days
const weeksCal = new Temporal.Calendar("iso8601");
TemporalHelpers.substituteMethod(weeksCal, "dateAdd", [
  TemporalHelpers.SUBSTITUTE_SKIP,  // Duration.total step 19.a MoveRelativeZonedDateTime → AddZonedDateTime
  TemporalHelpers.SUBSTITUTE_SKIP,  // RoundDuration step 7.a.i MoveRelativeZonedDateTime → AddZonedDateTime
  new Temporal.PlainDate(275760, 9, 13),  // RoundDuration step 12.q MoveRelativeDate
  // sets one{Year,Month,Week}Days to 200_000_000
]);
relativeTo = new Temporal.ZonedDateTime(-86400_0000_0000_000_000_000n, createTimeZone(), weeksCal);
const dWeeks = new Temporal.Duration(0, 0, /* weejs = */ 1, /* days = */ 1, 0, 0, 0, /* milliseconds = */ 199, 0, /* nanoseconds = */ 1);
assert.sameValue(dWeeks.total({ unit: "weeks", relativeTo }), expected, "Correct division by large number in weeks total");
