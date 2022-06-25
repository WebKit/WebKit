// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindatetime.prototype.daysinmonth
description: Checking days in month for a "normal" case (non-undefined, non-boundary case, etc.)
features: [Temporal]
---*/

const tests = [
  [new Temporal.PlainDateTime(1976, 2, 18, 15, 23, 30, 123, 456, 789), 29],
  [new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789), 30],
  [new Temporal.PlainDateTime(1976, 12, 18, 15, 23, 30, 123, 456, 789), 31],
  [new Temporal.PlainDateTime(1977, 2, 18, 15, 23, 30, 123, 456, 789), 28],
];
for (const [plainDateTime, expected] of tests) {
  assert.sameValue(plainDateTime.daysInMonth, expected, `${expected} days in the month of ${plainDateTime}`);
}
