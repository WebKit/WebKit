// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: A number as calendar in a property bag is converted to a string, then to a calendar
features: [Temporal]
---*/

const calendar = 19970327;

const arg = { year: 2019, monthCode: "M06", calendar };
const result1 = Temporal.PlainYearMonth.compare(arg, new Temporal.PlainYearMonth(2019, 6));
assert.sameValue(result1, 0, "19970327 is a valid ISO string for calendar (first argument)");
const result2 = Temporal.PlainYearMonth.compare(new Temporal.PlainYearMonth(2019, 6), arg);
assert.sameValue(result2, 0, "19970327 is a valid ISO string for calendar (second argument)");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 2019, monthCode: "M06", calendar };
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.compare(arg, new Temporal.PlainYearMonth(2019, 6)),
    `Number ${calendar} does not convert to a valid ISO string for calendar (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.compare(new Temporal.PlainYearMonth(2019, 6), arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar (second argument)`
  );
}
