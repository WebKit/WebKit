// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A number as calendar in a property bag is converted to a string, then to a calendar
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = 19970327;

let arg = { year: 2019, monthCode: "M06", calendar };
const result1 = Temporal.PlainYearMonth.from(arg);
TemporalHelpers.assertPlainYearMonth(result1, 2019, 6, "M06", "19970327 is a valid ISO string for calendar");

arg = { year: 2019, monthCode: "M06", calendar: { calendar } };
const result2 = Temporal.PlainYearMonth.from(arg);
TemporalHelpers.assertPlainYearMonth(result2, 2019, 6, "M06", "19970327 is a valid ISO string for calendar (nested property)");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  let arg = { year: 2019, monthCode: "M06", calendar };
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.from(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar`
  );
  arg = { year: 2019, monthCode: "M06", calendar: { calendar } };
  assert.throws(
    RangeError,
    () => Temporal.PlainYearMonth.from(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar (nested property)`
  );
}
