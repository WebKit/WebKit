// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.equals
description: A number is converted to a string, then to Temporal.PlainYearMonth
features: [Temporal]
---*/

const instance = new Temporal.PlainYearMonth(2019, 6);

const arg = 201906;

const result = instance.equals(arg);
assert.sameValue(result, true, "201906 is a valid ISO string for PlainYearMonth");

const numbers = [
  1,
  -201906,
  1234567,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.equals(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainYearMonth`
  );
}
