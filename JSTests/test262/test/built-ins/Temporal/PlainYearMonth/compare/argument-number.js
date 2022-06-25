// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: A number is converted to a string, then to Temporal.PlainYearMonth
features: [Temporal]
---*/

const arg = 201906;

const result1 = Temporal.PlainYearMonth.compare(arg, new Temporal.PlainYearMonth(2019, 6));
assert.sameValue(result1, 0, "201906 is a valid ISO string for PlainYearMonth (first argument)");
const result2 = Temporal.PlainYearMonth.compare(new Temporal.PlainYearMonth(2019, 6), arg);
assert.sameValue(result2, 0, "201906 is a valid ISO string for PlainYearMonth (second argument)");

const numbers = [
  1,
  -201906,
  1234567,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.compare(arg, new Temporal.PlainYearMonth(2019, 6)),
    `Number ${arg} does not convert to a valid ISO string for PlainYearMonth (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.compare(new Temporal.PlainYearMonth(2019, 6), arg),
    `Number ${arg} does not convert to a valid ISO string for PlainYearMonth (first argument)`
  );
}
