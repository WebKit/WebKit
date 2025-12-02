// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: PlainMonthDay can be created for common leap month codes in Chinese calendar
features: [Temporal, Intl.Era-monthcode]
---*/

// Test common leap months in the Chinese calendar
// The Chinese calendar is a lunisolar calendar where leap months are inserted
// to keep the lunar year synchronized with the solar year.
// Common leap months (occurring more frequently in the astronomical cycle):
// M02L, M03L, M04L, M05L, M06L, M07L, M08L
//
// The distribution of leap months follows astronomical calculations.
//
// Note: M01L, M02L and M08L through M12L are temporarily omitted from this
// test because while any leap month can have 30 days, there isn't a year in the
// supported range where these months do, and it's not yet well-defined what
// reference ISO year should be used.
// See https://github.com/tc39/proposal-intl-era-monthcode/issues/60

const calendar = "chinese";

// Test leap months M03L-M07L with day 30
// These are well-established leap months that can have 30 days
const leapMonthsWith30Days = ["M03L", "M04L", "M05L", "M06L", "M07L"];

for (const monthCode of leapMonthsWith30Days) {
  // Test creation with monthCode
  const pmd = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 1 });
  assert.sameValue(pmd.monthCode, monthCode, `leap monthCode ${monthCode} should be preserved`);
  assert.sameValue(pmd.day, 1, `day should be 1 for ${monthCode}`);

  // These leap months can have up to 30 days (minimum for PlainMonthDay)
  const pmd30 = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 30 });
  assert.sameValue(pmd30.monthCode, monthCode, `${monthCode} with day 30 should be valid`);
  assert.sameValue(pmd30.day, 30, `day should be 30 for ${monthCode}`);

  // Test constrain overflow - day 31 should be constrained to 30
  const constrained = Temporal.PlainMonthDay.from(
    { calendar, monthCode, day: 31 },
    { overflow: "constrain" }
  );
  assert.sameValue(constrained.monthCode, monthCode, `${monthCode} should be preserved with constrain`);
  assert.sameValue(constrained.day, 30, `day 31 should be constrained to 30 for ${monthCode}`);

  // Test reject overflow - day 31 should throw
  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({ calendar, monthCode, day: 31 }, { overflow: "reject" });
  }, `${monthCode} with day 31 should throw with reject overflow`);
}
