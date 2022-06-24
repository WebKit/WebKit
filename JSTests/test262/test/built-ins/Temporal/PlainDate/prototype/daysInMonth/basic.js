// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.daysinmonth
description: Checking days in month for a "normal" case (non-undefined, non-boundary case, etc.)
features: [Temporal]
---*/

const tests = [
  [new Temporal.PlainDate(1976, 2, 18), 29],
  [new Temporal.PlainDate(1976, 11, 18), 30],
  [new Temporal.PlainDate(1976, 12, 18), 31],
  [new Temporal.PlainDate(1977, 2, 18), 28],
];
for (const [plainDate, expected] of tests) {
  assert.sameValue(plainDate.daysInMonth, expected, `${expected} days in the month of ${plainDate}`);
}
