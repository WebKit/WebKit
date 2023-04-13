// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.daysinweek
description: Validate result returned from calendar daysInWeek() method
features: [Temporal]
---*/

const badResults = [
  [undefined, TypeError],
  [null, TypeError],
  [false, TypeError],
  [Infinity, RangeError],
  [-Infinity, RangeError],
  [NaN, RangeError],
  [-7, RangeError],
  [-0.1, RangeError],
  ["string", TypeError],
  [Symbol("foo"), TypeError],
  [7n, TypeError],
  [{}, TypeError],
  [true, TypeError],
  [7.1, RangeError],
  ["7", TypeError],
  ["7.5", TypeError],
  [{valueOf() { return 7; }}, TypeError],
];

badResults.forEach(([result, error]) => {
  const calendar = new class extends Temporal.Calendar {
    daysInWeek() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
  assert.throws(error, () => instance.daysInWeek, `${typeof result} ${String(result)} not converted to positive integer`);
});
