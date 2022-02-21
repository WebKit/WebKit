// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: A PlainYearMonth argument is cloned
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const original = new Temporal.PlainYearMonth(2019, 11, undefined, 7);
const result = Temporal.PlainYearMonth.from(original);
assert.notSameValue(result, original);

for (const plainYearMonth of [original, result]) {
  TemporalHelpers.assertPlainYearMonth(plainYearMonth, 2019, 11, "M11");
  const fields = plainYearMonth.getISOFields();
  assert.sameValue(fields.calendar.id, "iso8601");
  assert.sameValue(fields.isoDay, 7, "isoDay");
  assert.sameValue(fields.isoMonth, 11, "isoMonth");
  assert.sameValue(fields.isoYear, 2019, "isoYear");
}
