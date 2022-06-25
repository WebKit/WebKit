// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A string is parsed into the correct object when passed as the argument
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const inputs = [
  "1976-11",
  "1976-11-10",
  "1976-11-01T09:00:00+00:00",
  "1976-11-01T00:00:00+05:00",
  "197611",
  "+00197611",
  "1976-11-18T15:23:30.1\u221202:00",
  "1976-11-18T152330.1+00:00",
  "19761118T15:23:30.1+00:00",
  "1976-11-18T15:23:30.1+0000",
  "1976-11-18T152330.1+0000",
  "19761118T15:23:30.1+0000",
  "19761118T152330.1+00:00",
  "19761118T152330.1+0000",
  "+001976-11-18T152330.1+00:00",
  "+0019761118T15:23:30.1+00:00",
  "+001976-11-18T15:23:30.1+0000",
  "+001976-11-18T152330.1+0000",
  "+0019761118T15:23:30.1+0000",
  "+0019761118T152330.1+00:00",
  "+0019761118T152330.1+0000",
  "1976-11-18T15:23",
  "1976-11-18T15",
  "1976-11-18",
];

for (const input of inputs) {
  const plainYearMonth = Temporal.PlainYearMonth.from(input);
  TemporalHelpers.assertPlainYearMonth(plainYearMonth, 1976, 11, "M11");
  const fields = plainYearMonth.getISOFields();
  assert.sameValue(fields.calendar.id, "iso8601");
  assert.sameValue(fields.isoDay, 1, "isoDay");
  assert.sameValue(fields.isoMonth, 11, "isoMonth");
  assert.sameValue(fields.isoYear, 1976, "isoYear");
}

const plainYearMonth = Temporal.PlainYearMonth.from("\u2212009999-11");
TemporalHelpers.assertPlainYearMonth(plainYearMonth, -9999, 11, "M11");
const fields = plainYearMonth.getISOFields();
assert.sameValue(fields.calendar.id, "iso8601");
assert.sameValue(fields.isoDay, 1, "isoDay");
assert.sameValue(fields.isoMonth, 11, "isoMonth");
assert.sameValue(fields.isoYear, -9999, "isoYear");
assert.sameValue(plainYearMonth.toString(), "-009999-11");
