// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: A number is converted to a string, then to Temporal.PlainTime
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(2000, 5, 2);

const arg = 123456.987654321;

const result = instance.toZonedDateTime({ plainTime: arg, timeZone: "UTC" });
assert.sameValue(result.epochNanoseconds, 957270896_987_654_321n, "123456.987654321 is a valid ISO string for PlainTime");

const numbers = [
  1,
  -123456.987654321,
  1234567,
  123456.9876543219,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.toZonedDateTime({ plainTime: arg, timeZone: "UTC" }),
    `Number ${arg} does not convert to a valid ISO string for PlainTime`
  );
}
