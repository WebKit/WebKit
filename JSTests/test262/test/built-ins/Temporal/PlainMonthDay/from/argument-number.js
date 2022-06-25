// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: A number is converted to a string, then to Temporal.PlainMonthDay
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const arg = 1118;

const result = Temporal.PlainMonthDay.from(arg);
TemporalHelpers.assertPlainMonthDay(result, "M11", 18, "1118 is a valid ISO string for PlainMonthDay");

const numbers = [
  1,
  -1118,
  12345,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainMonthDay.from(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainMonthDay`
  );
}
