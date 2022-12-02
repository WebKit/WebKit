// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plainyearmonth.prototype.monthsinyear
description: Validate result returned from calendar monthsInYear() method
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
    monthsInYear() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.PlainYearMonth(1981, 12, calendar);
  assert.throws(error, () => instance.monthsInYear, `${typeof result} not converted to positive integer`);
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
    monthsInYear() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.PlainYearMonth(1981, 12, calendar);
  assert.sameValue(instance.monthsInYear, convertedResult, `${typeof result} converted to positive integer ${convertedResult}`);
});
