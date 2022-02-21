// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tostring
description: Valid units for the smallestUnit option
features: [Temporal]
---*/

const time = new Temporal.PlainTime(12, 34, 56, 789, 999, 999);
assert.sameValue(time.toString({ smallestUnit: "minute" }), "12:34");
assert.sameValue(time.toString({ smallestUnit: "second" }), "12:34:56");
assert.sameValue(time.toString({ smallestUnit: "millisecond" }), "12:34:56.789");
assert.sameValue(time.toString({ smallestUnit: "microsecond" }), "12:34:56.789999");
assert.sameValue(time.toString({ smallestUnit: "nanosecond" }), "12:34:56.789999999");

const time2 = new Temporal.PlainTime(12, 34);
assert.sameValue(time2.toString({ smallestUnit: "minute" }), "12:34");
assert.sameValue(time2.toString({ smallestUnit: "second" }), "12:34:00");
assert.sameValue(time2.toString({ smallestUnit: "millisecond" }), "12:34:00.000");
assert.sameValue(time2.toString({ smallestUnit: "microsecond" }), "12:34:00.000000");
assert.sameValue(time2.toString({ smallestUnit: "nanosecond" }), "12:34:00.000000000");

const notValid = [
  "year",
  "month",
  "week",
  "day",
  "hour",
];

notValid.forEach((smallestUnit) => {
  assert.throws(RangeError, () => time.toString({ smallestUnit }), smallestUnit);
});
