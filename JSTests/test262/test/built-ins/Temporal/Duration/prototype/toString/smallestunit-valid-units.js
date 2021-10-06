// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.tostring
description: Valid units for the smallestUnit option
features: [Temporal]
---*/

const duration = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 987, 654, 321);

assert.sameValue(duration.toString({ smallestUnit: "second" }), "P1Y2M3W4DT5H6M7S");
assert.sameValue(duration.toString({ smallestUnit: "millisecond" }), "P1Y2M3W4DT5H6M7.987S");
assert.sameValue(duration.toString({ smallestUnit: "microsecond" }), "P1Y2M3W4DT5H6M7.987654S");
assert.sameValue(duration.toString({ smallestUnit: "nanosecond" }), "P1Y2M3W4DT5H6M7.987654321S");

const notValid = [
  "year",
  "month",
  "week",
  "day",
  "hour",
  "minute",
];

notValid.forEach((smallestUnit) => {
  assert.throws(RangeError, () => duration.toString({ smallestUnit }), smallestUnit);
});
