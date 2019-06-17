// Copyright 2019 Googe Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: Checks the order of getting options of 'quarter' for the DateTimeFormat constructor.
info: |
    ToDateTimeOptions ( options, required, defaults )
    4. If required is "date" or "any", then
      a. For each of the property names "weekday", "year", "quarter", "month",  "day", do
includes: [compareArray.js]
features: [Intl.DateTimeFormat-quarter]
---*/

// Just need to ensure quarter are get between year and month.
const expected = [
  // ToDateTimeOptions step 4.
  "year", "quarter", "month",
  // InitializeDateTimeFormat step 22.
  "year",
  "quarter",
  "month"
];

const actual = [];

const options = {
  get month() {
    actual.push("month");
    return "numeric";
  },
  get quarter() {
    actual.push("quarter");
    return "long";
  },
  get year() {
    actual.push("year");
    return "numeric";
  },
};

new Intl.DateTimeFormat("en", options);
assert.compareArray(actual, expected);
