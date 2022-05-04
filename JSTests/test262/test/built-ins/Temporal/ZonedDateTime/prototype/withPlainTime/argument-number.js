// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaintime
description: A number is converted to a string, then to Temporal.PlainTime
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC");

const arg = 123456.987654321;

const result = instance.withPlainTime(arg);
assert.sameValue(result.epochNanoseconds, 1000038896_987_654_321n, "123456.987654321 is a valid ISO string for PlainTime");

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
