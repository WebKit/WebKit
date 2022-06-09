// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tostring
description: Valid units for the smallestUnit option
features: [Temporal]
---*/

const datetime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 789, 999, 999);

assert.sameValue(datetime.toString({ smallestUnit: "minute" }), "2000-05-02T12:34");
assert.sameValue(datetime.toString({ smallestUnit: "second" }), "2000-05-02T12:34:56");
assert.sameValue(datetime.toString({ smallestUnit: "millisecond" }), "2000-05-02T12:34:56.789");
assert.sameValue(datetime.toString({ smallestUnit: "microsecond" }), "2000-05-02T12:34:56.789999");
assert.sameValue(datetime.toString({ smallestUnit: "nanosecond" }), "2000-05-02T12:34:56.789999999");

const notValid = [
  "year",
  "month",
  "week",
  "day",
  "hour",
];

notValid.forEach((smallestUnit) => {
  assert.throws(RangeError, () => datetime.toString({ smallestUnit }), smallestUnit);
});
