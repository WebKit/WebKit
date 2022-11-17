// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plainyearmonth.prototype.monthcode
description: Validate result returned from calendar monthCode() method
features: [Temporal]
---*/

const badResults = [
  [undefined, RangeError],
  [Symbol("foo"), TypeError],
];

badResults.forEach(([result, error]) => {
  const calendar = new class extends Temporal.Calendar {
    monthCode() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.PlainYearMonth(1981, 12, calendar);
  assert.throws(error, () => instance.monthCode, `${typeof result} not converted to string`);
});

const convertedResults = [
  [null, "null"],
  [true, "true"],
  [false, "false"],
  [7.1, "7.1"],
  ["M01", "M01"],
  [{toString() { return "M01"; }}, "M01"],
];

convertedResults.forEach(([result, convertedResult]) => {
  const calendar = new class extends Temporal.Calendar {
    monthCode() {
      return result;
    }
  }("iso8601");
  const instance = new Temporal.PlainYearMonth(1981, 12, calendar);
  assert.sameValue(instance.monthCode, convertedResult, `${typeof result} converted to string ${convertedResult}`);
});
