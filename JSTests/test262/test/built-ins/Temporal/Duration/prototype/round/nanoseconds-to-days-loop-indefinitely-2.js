// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  NanosecondsToDays can loop indefinitely.
info: |
  NanosecondsToDays ( nanoseconds, relativeTo )

  ...
  18. Repeat, while done is false,
    a. Let oneDayFartherNs be ℝ(? AddZonedDateTime(ℤ(intermediateNs), relativeTo.[[TimeZone]],
       relativeTo.[[Calendar]], 0, 0, 0, sign, 0, 0, 0, 0, 0, 0)).
    b. Set dayLengthNs to oneDayFartherNs - intermediateNs.
    c. If (nanoseconds - dayLengthNs) × sign ≥ 0, then
        i. Set nanoseconds to nanoseconds - dayLengthNs.
        ii. Set intermediateNs to oneDayFartherNs.
        iii. Set days to days + sign.
    d. Else,
        i. Set done to true.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

// Intentionally not Test262Error to ensure assertion errors are correctly propagated.
class StopExecution extends Error {}

const stopAt = 1000;

// Number of calls to getPossibleInstantsFor.
let count = 0;

// UTC time zones so we don't have to worry about time zone offsets.
let tz = new class extends Temporal.TimeZone {
  getPossibleInstantsFor(dt) {
    // Stop when we've reached the test limit.
    count += 1;
    if (count === stopAt) {
      throw new StopExecution();
    }

    if (count < 4) {
      // The first couple calls request the instant for 1970-01-02.
      TemporalHelpers.assertPlainDateTime(dt, 1970, 1, "M01", 2, 0, 0, 0, 0, 0, 0);
    } else {
      // Later on the instant for 1970-01-03 is requested.
      TemporalHelpers.assertPlainDateTime(dt, 1970, 1, "M01", 3, 0, 0, 0, 0, 0, 0);
    }

    // Always return "1970-01-02T00:00:00Z". This leads to iterating indefinitely
    // in NanosecondsToDays.
    return [new Temporal.Instant(86400000000000n)];
  }
}("UTC");

let zdt = new Temporal.ZonedDateTime(0n, tz);
let duration = Temporal.Duration.from({days: 1});
let options = {
  smallestUnit: "days",
  relativeTo: zdt,
};

assert.throws(StopExecution, () => duration.round(options));
assert.sameValue(count, stopAt);
