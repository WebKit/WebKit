// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: >
  NanosecondsToDays computes with precise mathematical integers.
info: |
  NanosecondsToDays ( nanoseconds, relativeTo )
  ...
  17. Repeat, while done is false,
    ...
    c. If (nanoseconds - dayLengthNs) × sign ≥ 0, then
      ...
      iii. Set days to days + sign.

  Ensure |days = days + sign| is exact and doesn't loose precision.
features: [Temporal]
---*/

var cal = new class extends Temporal.Calendar {
  #dateUntil = 0;

  dateUntil(one, two, options) {
    if (++this.#dateUntil === 1) {
      return Temporal.Duration.from({days: Number.MAX_SAFE_INTEGER + 10});
    }
    return super.dateUntil(one, two, options);
  }

  #dateAdd = 0;

  dateAdd(date, duration, options) {
    if (++this.#dateAdd === 3) {
      return super.dateAdd(date, "P1D", options);
    }
    return super.dateAdd(date, duration, options);
  }
}("iso8601");

var zoned = new Temporal.ZonedDateTime(0n, "UTC", cal);
var duration = Temporal.Duration.from({days: 5});
var result = duration.round({smallestUnit: "days", relativeTo: zoned});

assert.sameValue(result.days, Number(BigInt(Number.MAX_SAFE_INTEGER + 10) + 5n));
