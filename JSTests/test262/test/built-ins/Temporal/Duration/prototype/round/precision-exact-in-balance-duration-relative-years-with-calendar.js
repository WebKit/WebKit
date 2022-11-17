// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  BalanceDurationRelative computes on exact mathematical values.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let calendar = new class extends Temporal.Calendar {
  #dateUntil = 0;
  dateUntil(one, two, options) {
    this.#dateUntil++;

    // Subtract one six times from 9007199254740996.
    if (this.#dateUntil <= 6) {
      return Temporal.Duration.from({months: 1});
    }

    // After subtracting six in total, months is 9007199254740996 - 6 = 9007199254740990.
    // |MAX_SAFE_INTEGER| = 9007199254740991 is larger than 9007199254740990, so we exit
    // from the loop.
    if (this.#dateUntil === 7) {
      return Temporal.Duration.from({months: Number.MAX_SAFE_INTEGER});
    }

    // Any additional calls to dateUntil are incorrect.
    throw new Test262Error("dateUntil called more times than expected");
  }
}("iso8601");

let date = new Temporal.PlainDate(1970, 1, 1, calendar);

let duration = Temporal.Duration.from({
  years: 0,
  months: Number.MAX_SAFE_INTEGER + 4, // 9007199254740996
});

let result = duration.round({
  largestUnit: "years",
  relativeTo: date,
});

// Years is equal to the number of times we returned one month from dateUntil.
// Months is equal to 9007199254740996 - 6.
TemporalHelpers.assertDuration(
  result,
  6, 9007199254740990, 0, 0,
  0, 0, 0,
  0, 0, 0,
);
