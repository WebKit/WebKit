// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.weekofyear
description: Validate result returned from calendar weekOfYear() method
features: [Temporal]
---*/

const badResults = [
  [undefined, RangeError],
  [null, RangeError],
  [false, RangeError],
  [Infinity, RangeError],
  [-Infinity, RangeError],
  [NaN, RangeError],
  [-7, RangeError],
  [-0.1, RangeError],
  ["string", RangeError],
  [Symbol("foo"), TypeError],
  [7n, TypeError],
  [{}, RangeError],
];

badResults.forEach(([result, error]) => {
  const calendar = new class extends Temporal.Calendar {
    weekOfYear() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
  assert.throws(error, () => instance.weekOfYear, `${typeof result} not converted to positive integer`);
});

const convertedResults = [
  [true, 1],
  [7.1, 7],
  ["7", 7],
  ["7.5", 7],
  [{valueOf() { return 7; }}, 7],
];

convertedResults.forEach(([result, convertedResult]) => {
  const calendar = new class extends Temporal.Calendar {
    weekOfYear() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
  assert.sameValue(instance.weekOfYear, convertedResult, `${typeof result} converted to positive integer ${convertedResult}`);
});
