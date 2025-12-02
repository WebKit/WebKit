// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
features: [Temporal]
description: Check correct results for 30-day leap months
includes: [temporalHelpers.js]
---*/

// Common leap months should find a result not too far into the past.
//
// Month -> ISO year
//
// M01L     <uncommon>
// M02L     1765
// M03L     1955
// M04L     1944
// M05L     1952
// M06L     1941
// M07L     1938
// M08L     1718
// M09L     <uncommon>
// M10L     <uncommon>
// M11L     <uncommon>
// M12L     <uncommon>
//
// M02L and M08L with 29 days is common, but with 30 is actually rather uncommon.
//
// See also "The Mathematics of the Chinese Calendar", Table 21 [1] for a
// distribution of leap months.
//
// [1] https://www.xirugu.com/CHI500/Dates_Time/Chinesecalender.pdf

// In this test, we skip the months whose most recent year is outside
// the range 1900-2100.
const monthCodesWithYears = [
  { monthCode: "M03L", referenceYear: 1955 },
  { monthCode: "M04L", referenceYear: 1944 },
  { monthCode: "M05L", referenceYear: 1952 },
  { monthCode: "M06L", referenceYear: 1941 },
  { monthCode: "M07L", referenceYear: 1938 }
];

const calendar = "chinese";

// Months can have up to 30 days.
const day = 30;

for (let {monthCode, referenceYear} of monthCodesWithYears) {
  let pmd = Temporal.PlainMonthDay.from({calendar, monthCode, day});
  TemporalHelpers.assertPlainMonthDay(pmd, monthCode, day, monthCode, referenceYear);

  let constrain = Temporal.PlainMonthDay.from({calendar, monthCode, day: day + 1}, {overflow: "constrain"});
  TemporalHelpers.assertPlainMonthDay(constrain, monthCode, day, `${monthCode} (constrained)`, referenceYear);
  assert.sameValue(constrain.equals(pmd), true);

  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({calendar, monthCode, day: day + 1}, {overflow: "reject"});
  });
}

