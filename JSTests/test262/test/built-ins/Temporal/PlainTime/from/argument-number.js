// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.from
description: A number is converted to a string, then to Temporal.PlainTime
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const arg = 123456.987654321;

const result = Temporal.PlainTime.from(arg);
TemporalHelpers.assertPlainTime(result, 12, 34, 56, 987, 654, 321, "123456.987654321 is a valid ISO string for PlainTime");

const numbers = [
  1,
  -123456.987654321,
  1234567,
  123456.9876543219,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainTime.from(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainTime`
  );
}
