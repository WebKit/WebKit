// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: PlainMonthDay can be created for all month codes (M01-M12) in Persian calendar
features: [Temporal, Intl.Era-monthcode]
---*/

// Test that all month codes M01-M12 are valid for the Persian calendar
// Persian calendar month lengths:
// - M01-M06: 31 days
// - M07-M12: 30 days

const calendar = "persian";

// Months with 31 days
const monthsWith31Days = ["M01", "M02", "M03", "M04", "M05", "M06"];

for (const monthCode of monthsWith31Days) {
  const pmd = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 1 });
  assert.sameValue(pmd.monthCode, monthCode, `monthCode ${monthCode} should be preserved`);
  assert.sameValue(pmd.day, 1, `day should be 1 for ${monthCode}`);

  const pmd31 = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 31 });
  assert.sameValue(pmd31.monthCode, monthCode, `${monthCode} with day 31 should be valid`);
  assert.sameValue(pmd31.day, 31, `day should be 31 for ${monthCode}`);

  const constrained = Temporal.PlainMonthDay.from(
    { calendar, monthCode, day: 32 },
    { overflow: "constrain" }
  );
  assert.sameValue(constrained.monthCode, monthCode, `${monthCode} should be preserved with constrain`);
  assert.sameValue(constrained.day, 31, `day 32 should be constrained to 31 for ${monthCode}`);

  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({ calendar, monthCode, day: 32 }, { overflow: "reject" });
  }, `${monthCode} with day 32 should throw with reject overflow`);
}

// Months with 30 days
const monthsWith30Days = ["M07", "M08", "M09", "M10", "M11", "M12"];

for (const monthCode of monthsWith30Days) {
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
