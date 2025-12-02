// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: PlainMonthDay can be created for all month codes (M01-M12) in Islamic-umalqura calendar
features: [Temporal, Intl.Era-monthcode]
---*/

// Test that all month codes M01-M12 are valid for the Islamic-umalqura calendar
// Unlike the tabular islamic-civil and islamic-tbla calendars, islamic-umalqura
// is an observational calendar where any month can have either 29 or 30 days
// depending on the year. Therefore, all months support up to 30 days for PlainMonthDay.

const calendar = "islamic-umalqura";

const allMonths = ["M01", "M02", "M03", "M04", "M05", "M06", "M07", "M08", "M09", "M10", "M11", "M12"];

for (const monthCode of allMonths) {
  const pmd = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 1 });
  assert.sameValue(pmd.monthCode, monthCode, `monthCode ${monthCode} should be preserved`);
  assert.sameValue(pmd.day, 1, `day should be 1 for ${monthCode}`);

  const pmd30 = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 30 });
  assert.sameValue(pmd30.monthCode, monthCode, `${monthCode} with day 30 should be valid`);
  assert.sameValue(pmd30.day, 30, `day should be 30 for ${monthCode}`);

  const constrained = Temporal.PlainMonthDay.from(
    { calendar, monthCode, day: 31 },
    { overflow: "constrain" }
  );
  assert.sameValue(constrained.monthCode, monthCode, `${monthCode} should be preserved with constrain`);
  assert.sameValue(constrained.day, 30, `day 31 should be constrained to 30 for ${monthCode}`);

  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({ calendar, monthCode, day: 31 }, { overflow: "reject" });
  }, `${monthCode} with day 31 should throw with reject overflow`);
}
