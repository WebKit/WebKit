// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: PlainMonthDay can be created for all regular month codes in Dangi calendar
features: [Temporal, Intl.Era-monthcode]
---*/

// Test that all regular month codes M01-M12 are valid for the Dangi calendar
// Dangi is a lunisolar calendar used in Korea, structurally identical to Chinese

const calendar = "dangi";
const monthCodes = [
  "M01", "M02", "M03", "M04", "M05", "M06",
  "M07", "M08", "M09", "M10", "M11", "M12"
];

for (const monthCode of monthCodes) {
  // Test creation with monthCode
  const pmd = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 1 });
  assert.sameValue(pmd.monthCode, monthCode, `monthCode ${monthCode} should be preserved`);
  assert.sameValue(pmd.day, 1, "day should be 1");

  // Test with a larger day value (months can have 29 or 30 days)
  const pmd30 = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 30 });
  assert.sameValue(pmd30.monthCode, monthCode, `monthCode ${monthCode} with day 30 should be valid`);

  // Test overflow: constrain to maximum valid day in the month
  const constrained = Temporal.PlainMonthDay.from(
    { calendar, monthCode, day: 31 },
    { overflow: "constrain" }
  );
  assert.sameValue(constrained.monthCode, monthCode, `monthCode ${monthCode} should be preserved with constrain`);
  assert.sameValue(constrained.day, 30, `day 31 should be constrained to 30 for ${monthCode}`);

  // Test overflow: reject should throw for invalid day
  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({ calendar, monthCode, day: 31 }, { overflow: "reject" });
  }, `monthCode ${monthCode} with day 31 should throw with reject overflow`);
}
