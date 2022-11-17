// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.daysinmonth
description: Validate result returned from calendar daysInMonth() method
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
    daysInMonth() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.PlainDate(1981, 12, 15, calendar);
  assert.throws(error, () => instance.daysInMonth, `${typeof result} not converted to positive integer`);
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
    daysInMonth() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.PlainDate(1981, 12, 15, calendar);
  assert.sameValue(instance.daysInMonth, convertedResult, `${typeof result} converted to positive integer ${convertedResult}`);
});
