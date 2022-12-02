// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: overflow property is extracted with string argument.
info: |
    1. Perform ? ToTemporalOverflow(_options_).
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
    return TemporalHelpers.toPrimitiveObserver(actual, "reject", "overflow");
  }
};

const result = Temporal.PlainDate.from("2021-05-17", object);
assert.compareArray(actual, expected, "Successful call");
TemporalHelpers.assertPlainDate(result, 2021, 5, "M05", 17);

actual.splice(0);  // empty it for the next check
assert.throws(RangeError, () => Temporal.PlainDate.from(7, object));
assert.compareArray(actual, expected, "Failing call");
