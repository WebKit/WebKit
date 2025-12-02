// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: PlainMonthDay can be created for all month codes (M01-M13) in Ethioaa calendar
features: [Temporal, Intl.Era-monthcode]
---*/

// Test that all month codes M01-M13 are valid for the Ethioaa calendar
// Ethioaa (also known as ethiopic-amete-alem) has the same structure as Ethiopic:
// 12 months of 30 days each, plus a 13th month (Pagumen) of 5 or 6 days

const calendar = "ethioaa";

// M01-M12: Regular months with 30 days each
const regularMonthCodes = [
  "M01", "M02", "M03", "M04", "M05", "M06",
  "M07", "M08", "M09", "M10", "M11", "M12"
];

for (const monthCode of regularMonthCodes) {
  // Test creation with monthCode
  const pmd = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 1 });
  assert.sameValue(pmd.monthCode, monthCode, `monthCode ${monthCode} should be preserved`);
  assert.sameValue(pmd.day, 1, "day should be 1");

  // Test with day 30 (all regular months have 30 days)
  const pmd30 = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 30 });
  assert.sameValue(pmd30.monthCode, monthCode, `${monthCode} with day 30 should be valid`);
  assert.sameValue(pmd30.day, 30, `day should be 30 for ${monthCode}`);

  // Test overflow: constrain to 30
  const constrained = Temporal.PlainMonthDay.from(
    { calendar, monthCode, day: 31 },
    { overflow: "constrain" }
  );
  assert.sameValue(constrained.monthCode, monthCode, `${monthCode} should be preserved with constrain`);
  assert.sameValue(constrained.day, 30, `day 31 should be constrained to 30 for ${monthCode}`);

  // Test overflow: reject should throw for day 31
  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({ calendar, monthCode, day: 31 }, { overflow: "reject" });
  }, `${monthCode} with day 31 should throw with reject overflow`);
}

// M13: Short month (Pagumen) with 5 or 6 days

// Test M13 with day 6 (maximum, valid in leap years)
const pmdM13Day6 = Temporal.PlainMonthDay.from({ calendar, monthCode: "M13", day: 6 });
assert.sameValue(pmdM13Day6.monthCode, "M13", "M13 should be valid with day 6");
assert.sameValue(pmdM13Day6.day, 6, "day should be 6 for M13");

// Test M13 overflow: constrain to maximum
const constrained = Temporal.PlainMonthDay.from(
  { calendar, monthCode: "M13", day: 7 },
  { overflow: "constrain" }
);
assert.sameValue(constrained.monthCode, "M13", "M13 should be preserved with constrain");
assert.sameValue(constrained.day, 6, "day 7 should be constrained to 6 for M13");

// Test M13 overflow: reject should throw for day 7
assert.throws(RangeError, () => {
  Temporal.PlainMonthDay.from({ calendar, monthCode: "M13", day: 7 }, { overflow: "reject" });
}, "M13 with day 7 should throw with reject overflow");
