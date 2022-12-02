// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.yearofweek
description: Validate result returned from calendar yearOfWeek() method
features: [Temporal]
---*/

const badResults = [
  [undefined, RangeError],
  [Infinity, RangeError],
  [-Infinity, RangeError],
  [Symbol("foo"), TypeError],
  [7n, TypeError],
];

badResults.forEach(([result, error]) => {
  const calendar = new class extends Temporal.Calendar {
    yearOfWeek() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.PlainDate(1981, 12, 15, calendar);
  assert.throws(error, () => instance.yearOfWeek, `${typeof result} not converted to integer`);
});

const convertedResults = [
  [null, 0],
  [true, 1],
  [false, 0],
  [7.1, 7],
  [-7, -7],
  [-0.1, 0],
  [NaN, 0],
  ["string", 0],
  ["7", 7],
  ["7.5", 7],
  [{}, 0],
  [{valueOf() { return 7; }}, 7],
];

convertedResults.forEach(([result, convertedResult]) => {
  const calendar = new class extends Temporal.Calendar {
    yearOfWeek() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.PlainDate(1981, 12, 15, calendar);
  assert.sameValue(instance.yearOfWeek, convertedResult, `${typeof result} converted to integer ${convertedResult}`);
});
