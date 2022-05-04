// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: overflow property is extracted with ISO-invalid string argument.
info: |
    1. Perform ? ToTemporalOverflow(_options_).

    1. If ! IsValidISODate(year, month, day) is false, throw a RangeError exception.
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get overflow",
  "get overflow.toString",
  "call overflow.toString",
];

let actual = [];
const object = {
  get overflow() {
    actual.push("get overflow");
    return TemporalHelpers.toPrimitiveObserver(actual, "constrain", "overflow");
  }
};

assert.throws(RangeError, () => Temporal.PlainDate.from("2020-13-34", object));
assert.compareArray(actual, expected);
