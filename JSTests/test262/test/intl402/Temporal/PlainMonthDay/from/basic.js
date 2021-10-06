// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: Basic tests for PlainMonthDay.from() with the Gregorian calendar.
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const tests = [
  { era: "ce", eraYear: 2019, month: 11, day: 18, calendar: "gregory" },
  { era: "ce", eraYear: 1979, month: 11, day: 18, calendar: "gregory" },
  { era: "ce", eraYear: 2019, monthCode: "M11", day: 18, calendar: "gregory" },
  { era: "ce", eraYear: 1979, monthCode: "M11", day: 18, calendar: "gregory" },
  { year: 1970, month: 11, day: 18, calendar: "gregory" },
  { era: "ce", eraYear: 1970, month: 11, day: 18, calendar: "gregory" },
];

for (const fields of tests) {
  const result = Temporal.PlainMonthDay.from(fields);
  TemporalHelpers.assertPlainMonthDay(result, "M11", 18);
  assert.sameValue(result.calendar.id, "gregory");
  assert.sameValue(result.toString(), "1972-11-18[u-ca=gregory]");
}

assert.throws(TypeError, () => Temporal.PlainMonthDay.from({ month: 11, day: 18, calendar: "gregory" }));
