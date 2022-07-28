// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
  Relative to a ZonedDateTime with a fractional number of days and different sign.
features: [Temporal]
---*/

let duration = Temporal.Duration.from({
  weeks: 1,
  days: 0,
  hours: 1,
});

let cal = new class extends Temporal.Calendar {
  #dateAdd = 0;

  dateAdd(date, duration, options) {
    if (++this.#dateAdd === 1) {
      duration = "-P1W";
    }
    return super.dateAdd(date, duration, options);
  }
}("iso8601");

let zdt = new Temporal.ZonedDateTime(0n, "UTC", cal);

let result = duration.total({
  relativeTo: zdt,
  unit: "days",
});

assert.sameValue(result, -7 + 1 / 24);
