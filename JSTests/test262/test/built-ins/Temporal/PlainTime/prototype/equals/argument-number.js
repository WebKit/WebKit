// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.equals
description: A number is converted to a string, then to Temporal.PlainTime
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

const arg = 123456.987654321;

const result = instance.equals(arg);
assert.sameValue(result, true, "123456.987654321 is a valid ISO string for PlainTime");

const numbers = [
  1,
  -123456.987654321,
  1234567,
  123456.9876543219,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.equals(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainTime`
  );
}
