// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A string is parsed into the correct object when passed as the argument
includes: [temporalHelpers.js]
features: [Temporal]
---*/

for (const input of TemporalHelpers.ISO.plainYearMonthStringsValid()) {
  const plainYearMonth = Temporal.PlainYearMonth.from(input);
  TemporalHelpers.assertPlainYearMonth(plainYearMonth, 1976, 11, "M11");
  const fields = plainYearMonth.getISOFields();
  assert.sameValue(fields.calendar.id, "iso8601");
  assert.sameValue(fields.isoDay, 1, "isoDay");
  assert.sameValue(fields.isoMonth, 11, "isoMonth");
  assert.sameValue(fields.isoYear, 1976, "isoYear");
}

for (const input of TemporalHelpers.ISO.plainYearMonthStringsValidNegativeYear()) {
  const plainYearMonth = Temporal.PlainYearMonth.from(input);
  TemporalHelpers.assertPlainYearMonth(plainYearMonth, -9999, 11, "M11");
  const fields = plainYearMonth.getISOFields();
  assert.sameValue(fields.calendar.id, "iso8601");
  assert.sameValue(fields.isoDay, 1, "isoDay");
  assert.sameValue(fields.isoMonth, 11, "isoMonth");
  assert.sameValue(fields.isoYear, -9999, "isoYear");
  assert.sameValue(plainYearMonth.toString(), "-009999-11");
}
