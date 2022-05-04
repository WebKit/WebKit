// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A number is converted to a string, then to Temporal.PlainYearMonth
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const arg = 201906;

const result = Temporal.PlainYearMonth.from(arg);
TemporalHelpers.assertPlainYearMonth(result, 2019, 6, "M06", "201906 is a valid ISO string for PlainYearMonth");

const numbers = [
  1,
  -201906,
  1234567,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.from(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainYearMonth`
  );
}
