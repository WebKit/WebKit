// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.compare
description: A number is converted to a string, then to Temporal.PlainTime
features: [Temporal]
---*/

const arg = 123456.987654321;

const result1 = Temporal.PlainTime.compare(arg, new Temporal.PlainTime(12, 34, 56, 987, 654, 321));
assert.sameValue(result1, 0, "123456.987654321 is a valid ISO string for PlainTime (first argument)");
const result2 = Temporal.PlainTime.compare(new Temporal.PlainTime(12, 34, 56, 987, 654, 321), arg);
assert.sameValue(result2, 0, "123456.987654321 is a valid ISO string for PlainTime (second argument)");

const numbers = [
  1,
  -123456.987654321,
  1234567,
  123456.9876543219,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainTime.compare(arg, new Temporal.PlainTime(12, 34, 56, 987, 654, 321)),
    `Number ${arg} does not convert to a valid ISO string for PlainTime (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainTime.compare(new Temporal.PlainTime(12, 34, 56, 987, 654, 321), arg),
    `Number ${arg} does not convert to a valid ISO string for PlainTime (second argument)`
  );
}
