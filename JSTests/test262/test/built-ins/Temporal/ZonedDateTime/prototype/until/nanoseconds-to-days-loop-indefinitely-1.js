// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: >
  NanosecondsToDays can loop arbitrarily long, performing observable operations each iteration.
info: |
  NanosecondsToDays ( nanoseconds, relativeTo )

  ...
  15. If sign is 1, then
    a. Repeat, while days > 0 and intermediateNs > endNs,
        i. Set days to days - 1.
        ii. Set intermediateNs to ℝ(? AddZonedDateTime(ℤ(startNs), relativeTo.[[TimeZone]],
            relativeTo.[[Calendar]], 0, 0, 0, days, 0, 0, 0, 0, 0, 0)).
  ...
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calls = [];
const dayLengthNs = 86400000000000n;
const other = new Temporal.ZonedDateTime(dayLengthNs, "UTC", "iso8601");

function createRelativeTo(count) {
  const tz = new Temporal.TimeZone("UTC");
  // Record calls in calls[]
  TemporalHelpers.observeMethod(calls, tz, "getPossibleInstantsFor");
  const cal = new Temporal.Calendar("iso8601");
  // Return _count_ days for the second call to dateUntil, behaving normally after
  TemporalHelpers.substituteMethod(cal, "dateUntil", [
    TemporalHelpers.SUBSTITUTE_SKIP,
    Temporal.Duration.from({ days: count }),
  ]);
  return new Temporal.ZonedDateTime(0n, tz, cal);
}

let zdt = createRelativeTo(200);
calls.splice(0); // Reset calls list after ZonedDateTime construction
zdt.until(other, {
  largestUnit: "day",
});
assert.sameValue(
  calls.length,
  200 + 1,
  "Expected ZonedDateTime.until to call getPossibleInstantsFor correct number of times"
);

zdt = createRelativeTo(300);
calls.splice(0); // Reset calls list after previous loop + ZonedDateTime construction
zdt.until(other, {
  largestUnit: "day",
});
assert.sameValue(
  calls.length,
  300 + 1,
  "Expected ZonedDateTime.until to call getPossibleInstantsFor correct number of times"
);
