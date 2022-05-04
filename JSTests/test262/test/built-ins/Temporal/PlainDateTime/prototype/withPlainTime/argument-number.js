// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaintime
description: A number is converted to a string, then to Temporal.PlainTime
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(2000, 5, 2);

const arg = 123456.987654321;

const result = instance.withPlainTime(arg);
TemporalHelpers.assertPlainDateTime(result, 2000, 5, "M05", 2, 12, 34, 56, 987, 654, 321, "123456.987654321 is a valid ISO string for PlainTime");

const numbers = [
  1,
  -123456.987654321,
  1234567,
  123456.9876543219,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.withPlainTime(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainTime`
  );
}
