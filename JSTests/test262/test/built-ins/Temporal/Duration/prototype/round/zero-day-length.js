// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: A malicious time zone resulting a day length of zero is handled correctly
info: |
    Based on a test by André Bargull.

    RoundDuration step 6:
      d. Let _result_ be ? NanosecondsToDays(_nanoseconds_, _intermediate_).
      e. Set _days_ to _days_ + _result_.[[Days]] + _result_.[[Nanoseconds]] / _result_.[[DayLength]].

    NanosecondsToDays steps 19-23:
      19. If _days_ < 0 and _sign_ = 1, throw a *RangeError* exception.
      20. If _days_ > 0 and _sign_ = -1, throw a *RangeError* exception.
      21. If _nanoseconds_ < 0, then
        a. Assert: sign is -1.
      22. If _nanoseconds_ > 0 and _sign_ = -1, throw a *RangeError* exception.
      23. Assert: The inequality abs(_nanoseconds_) < abs(_dayLengthNs_) holds.
features: [Temporal]
---*/

const instance = new Temporal.Duration(0, 0, 0, 0, -24, 0, 0, 0, 0, -1);

const tz = new class extends Temporal.TimeZone {
  #getPossibleInstantsForCalls = 0;

  getPossibleInstantsFor(dt) {
    this.#getPossibleInstantsForCalls++;

    if (this.#getPossibleInstantsForCalls <= 2) {
      return [new Temporal.Instant(-86400_000_000_000n - 2n)]
    }
    return super.getPossibleInstantsFor(dt);
  }
}("UTC");

const relativeTo = new Temporal.ZonedDateTime(0n, tz, "iso8601");
assert.throws(RangeError, () => instance.round({ relativeTo, smallestUnit: "days" }));
