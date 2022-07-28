// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  BalanceDuration throws when the duration signs don't match.
info: |
  BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds,
                    largestUnit [ , relativeTo ] )

  ...
  4. If largestUnit is one of "year", "month", "week", or "day", then
    a. Let result be ? NanosecondsToDays(nanoseconds, relativeTo).
    b. Set days to result.[[Days]].
    c. Set nanoseconds to result.[[Nanoseconds]].
  ...
  15. Return ? CreateTimeDurationRecord(days, hours × sign, minutes × sign, seconds × sign,
      milliseconds × sign, microseconds × sign, nanoseconds × sign).
features: [Temporal]
---*/

let duration = Temporal.Duration.from({
  hours: -(24 * 1),
  nanoseconds: -1,
});

let tz = new class extends Temporal.TimeZone {
  #getPossibleInstantsFor = 0;

  getPossibleInstantsFor(dt) {
    this.#getPossibleInstantsFor++;

    if (this.#getPossibleInstantsFor === 1) {
      return [new Temporal.Instant(-86400_000_000_000n - 2n)]
    }
    return super.getPossibleInstantsFor(dt);
  }
}("UTC");

let zdt = new Temporal.ZonedDateTime(0n, tz, "iso8601");

let options = {
  relativeTo: zdt,
  largestUnit: "days",
};

assert.throws(RangeError, () => duration.round(options));
