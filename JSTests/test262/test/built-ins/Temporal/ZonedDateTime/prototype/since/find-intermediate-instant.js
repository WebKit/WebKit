// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: >
  Up to 3 intermediate instants may be tried when calculating ZonedDateTime
  difference
includes: [compareArray.js, temporalHelpers.js]
features: [BigInt, Temporal]
---*/

const calls = [];

const springFallZone = TemporalHelpers.springForwardFallBackTimeZone();
TemporalHelpers.observeMethod(calls, springFallZone, "getPossibleInstantsFor");

const dateLineZone = TemporalHelpers.crossDateLineTimeZone();
TemporalHelpers.observeMethod(calls, dateLineZone, "getPossibleInstantsFor");

const zdt2 = new Temporal.ZonedDateTime(946722600_000_000_000n /* = 2000-01-01T02:30 local */, springFallZone);

// Future -> past, without wall-clock overshoot
// Expects valid intermediate Instant WITHOUT day correction (computed once)
{
  const zdt1 = new Temporal.ZonedDateTime(949442400_000_000_000n /* = 2000-02-01T14:00 local */, springFallZone);
  const result = zdt1.since(zdt2, { largestUnit: "years" });
  TemporalHelpers.assertDuration(result, 0, 1, 0, 0, 11, 30, 0, 0, 0, 0, "no wall-clock overshoot, no DST");
  assert.compareArray(calls, [
    "call getPossibleInstantsFor",  // first intermediate in DifferenceZonedDateTime
  ], "one intermediate should be tried");
}

calls.splice(0);  // clear

// Future -> past, WITH wall-clock overshoot
// Expects valid intermediate Instant with guaranteed 1-DAY correction (computed once)
{
  const zdt1 = new Temporal.ZonedDateTime(949395600_000_000_000n /* = 2000-02-01T01:00 local */, springFallZone);
  const result = zdt1.since(zdt2, { largestUnit: "years" });
  TemporalHelpers.assertDuration(result, 0, 0, 0, 30, 22, 30, 0, 0, 0, 0, "wall-clock overshoot, no DST");
  assert.compareArray(calls, [
    "call getPossibleInstantsFor",  // first intermediate in DifferenceZonedDateTime
  ], "one intermediate should be tried");
}

calls.splice(0);  // clear

// Future -> past, WITH wall-clock overshoot
// Expects valid intermediate Instant with guaranteed 1-DAY correction (computed once)
// Intermediate Instant falls within spring DST gap and gets pushed forward,
// but since moving from future -> past, not possible to exacerbate overflow,
// so no other day corrections.
{
  const end = new Temporal.ZonedDateTime(957258000_000_000_000n /* = 2000-05-02T02:00 local */, springFallZone);
  const start = new Temporal.ZonedDateTime(954671400_000_000_000n /* = 2000-04-02T03:30-07:00 local */, springFallZone);
  const result = end.since(start, { largestUnit: "years" });
  TemporalHelpers.assertDuration(result, 0, 0, 0, 29, 22, 30, 0, 0, 0, 0, "wall-clock overshoot, inconsiquential DST");
  assert.compareArray(calls, [
    "call getPossibleInstantsFor",  // first intermediate in DifferenceZonedDateTime
  ], "one intermediate should be tried");
}

calls.splice(0);  // clear

// Past -> future, WITH wall-clock overshoot
// Tries intermediate Instant with 1-DAY correction (first compute)
// Then, ANOTHER day correction because updated intermediate Instant falls within dateline DST gap,
// pushing it forward, causing wall-clock overshoot again
// (Not possible when going backwards)
// (This test is just the same as the corresponding one in until(), but negative)
{
  const start = new Temporal.ZonedDateTime(1325102400_000_000_000n /* = 2011-12-28T10:00 local */, dateLineZone);
  const end = new Temporal.ZonedDateTime(1325257200_000_000_000n /* = 2011-12-31T05:00 local */, dateLineZone);
  const result = start.since(end, { largestUnit: "days" });
  TemporalHelpers.assertDuration(result, 0, 0, 0, -1, -19, 0, 0, 0, 0, 0, "wall-clock overshoot, consiquential DST");
  assert.compareArray(calls, [
    "call getPossibleInstantsFor",  // first intermediate in DifferenceZonedDateTime
    "call getPossibleInstantsFor",  // second intermediate in DifferenceZonedDateTime
    "call getPossibleInstantsFor",  // DisambiguatePossibleInstants on second intermediate
  ], "two intermediates should be tried, with disambiguation");
}
