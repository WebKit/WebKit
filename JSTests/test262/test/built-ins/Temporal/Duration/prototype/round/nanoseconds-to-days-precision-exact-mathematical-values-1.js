// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: >
  NanosecondsToDays computes with precise mathematical integers.
info: |
  NanosecondsToDays ( nanoseconds, relativeTo )
  ...
  14. If sign is 1, then
    a. Repeat, while days > 0 and intermediateNs > endNs,
      i. Set days to days - 1.
     ii. ...

  Ensure |days = days - 1| is exact and doesn't loose precision.
features: [Temporal]
---*/

var expectedDurationDays = [
  Number.MAX_SAFE_INTEGER + 4, // 9007199254740996
  Number.MAX_SAFE_INTEGER + 3, // 9007199254740994
  Number.MAX_SAFE_INTEGER + 2, // 9007199254740992
  Number.MAX_SAFE_INTEGER + 1, // 9007199254740992
  Number.MAX_SAFE_INTEGER + 0, // 9007199254740991
  Number.MAX_SAFE_INTEGER - 1, // 9007199254740990
  Number.MAX_SAFE_INTEGER - 2, // 9007199254740989
  Number.MAX_SAFE_INTEGER - 3, // 9007199254740988
  Number.MAX_SAFE_INTEGER - 4, // 9007199254740987
  Number.MAX_SAFE_INTEGER - 5, // 9007199254740986
];

// Intentionally not Test262Error to ensure assertions errors are propagated.
class StopExecution extends Error {}

var cal = new class extends Temporal.Calendar {
  #dateUntil = 0;

  dateUntil(one, two, options) {
    if (++this.#dateUntil === 1) {
      return Temporal.Duration.from({days: Number.MAX_SAFE_INTEGER + 4});
    }
    return super.dateUntil(one, two, options);
  }

  #dateAdd = 0;

  dateAdd(date, duration, options) {
    // Ensure we don't add too many days which would lead to creating an invalid date.
    if (++this.#dateAdd === 3) {
      assert.sameValue(duration.days, Number.MAX_SAFE_INTEGER + 4);

      // The added days must be larger than 5 for the |intermediateNs > endNs| condition.
      return super.dateAdd(date, "P6D", options);
    }

    // Ensure the duration days are exact.
    if (this.#dateAdd > 3) {
      if (!expectedDurationDays.length) {
        throw new StopExecution();
      }
      assert.sameValue(duration.days, expectedDurationDays.shift());

      // Add more than 5 for the |intermediateNs > endNs| condition.
      return super.dateAdd(date, "P6D", options);
    }

    // Otherwise call the default implementation.
    return super.dateAdd(date, duration, options);
  }
}("iso8601");

var zoned = new Temporal.ZonedDateTime(0n, "UTC", cal);
var duration = Temporal.Duration.from({days: 5});
var options = {smallestUnit: "days", relativeTo: zoned};

assert.throws(StopExecution, () => duration.round(options));
