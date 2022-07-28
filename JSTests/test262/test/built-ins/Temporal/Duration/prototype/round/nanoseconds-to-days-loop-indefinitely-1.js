// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  NanosecondsToDays can loop indefinitely.
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

// Intentionally not Test262Error to ensure assertion errors are correctly propagated.
class StopExecution extends Error {}

const stopAt = 1000;

// Always add two days to the start date, this ensures the intermediate date
// is larger than the end date.
let twoDays = Temporal.Duration.from({days: 2});

// Number of calls to dateAdd.
let count = 0;

let cal = new class extends Temporal.Calendar {
  // Set `days` to a number larger than `Number.MAX_SAFE_INTEGER`.
  dateUntil(start, end, options) {
    return Temporal.Duration.from({days: Number.MAX_VALUE});
  }

  dateAdd(date, duration, options) {
    // Stop when we've reached the test limit.
    count += 1;
    if (count === stopAt) {
      throw new StopExecution();
    }

    if (count === 1) {
      return Temporal.Calendar.prototype.dateAdd.call(this, date, duration, options);
    }

    TemporalHelpers.assertPlainDate(date, 1970, 1, "M01", 1);

    TemporalHelpers.assertDuration(
      duration,
      0, 0, 0, Number.MAX_VALUE,
      0, 0, 0,
      0, 0, 0,
    );

    return Temporal.Calendar.prototype.dateAdd.call(this, date, twoDays, options);
  }
}("iso8601");

let zdt = new Temporal.ZonedDateTime(0n, "UTC", cal);
let duration = Temporal.Duration.from({days: 1});
let options = {
  largestUnit: "days",
  relativeTo: zdt,
};

assert.throws(StopExecution, () => duration.round(options));
assert.sameValue(count, stopAt);
