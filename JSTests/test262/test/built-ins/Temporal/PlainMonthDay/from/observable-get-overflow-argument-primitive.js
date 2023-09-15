// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: overflow property is extracted with string argument.
info: |
    1. Perform ? ToTemporalOverflow(_options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "ownKeys options",
  "getOwnPropertyDescriptor options.overflow",
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
];

let actual = [];
const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

const result = Temporal.PlainMonthDay.from("05-17", options);
assert.compareArray(actual, expected, "Successful call");
TemporalHelpers.assertPlainMonthDay(result, "M05", 17);

actual.splice(0);  // empty it for the next check
const failureExpected = [
  "ownKeys options",
  "getOwnPropertyDescriptor options.overflow",
  "get options.overflow",
];

assert.throws(TypeError, () => Temporal.PlainMonthDay.from(7, options));
assert.compareArray(actual, failureExpected, "Failing call");
