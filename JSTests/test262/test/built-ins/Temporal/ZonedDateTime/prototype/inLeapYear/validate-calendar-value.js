// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.inleapyear
description: Validate result returned from calendar inLeapYear() method
features: [Temporal]
---*/

const convertedResults = [
  [undefined, false],
  [null, false],
  [true, true],
  [false, false],
  [0, false],
  [-0, false],
  [42, true],
  [7.1, true],
  [NaN, false],
  [Infinity, true],
  [-Infinity, true],
  ["", false],
  ["a string", true],
  ["0", true],
  [Symbol("foo"), true],
  [0n, false],
  [42n, true],
  [{}, true],
  [{valueOf() { return false; }}, true],
];

convertedResults.forEach(([result, convertedResult]) => {
  const calendar = new class extends Temporal.Calendar {
    inLeapYear() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
  assert.sameValue(instance.inLeapYear, convertedResult, `${typeof result} converted to boolean ${convertedResult}`);
});
