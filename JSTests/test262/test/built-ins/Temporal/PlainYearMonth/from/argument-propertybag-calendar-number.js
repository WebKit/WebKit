// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A number as calendar in a property bag is not accepted
features: [Temporal]
---*/

const numbers = [
  1,
  19970327,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 2019, monthCode: "M06", calendar };
  assert.throws(
    TypeError,
    () => Temporal.PlainYearMonth.from(arg),
    "Numbers cannot be used as a calendar"
  );
}
