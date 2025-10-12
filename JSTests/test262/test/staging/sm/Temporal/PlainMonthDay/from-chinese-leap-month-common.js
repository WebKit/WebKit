// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-Temporal-PlainMonthDay-shell.js]
features:
  - Temporal
description: |
  pending
esid: pending
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

const monthCodes = [
  // M01L is an uncommon leap month.
  "M02L",
  "M03L",
  "M04L",
  "M05L",
  "M06L",
  "M07L",
  "M08L",
  // M09L..M12L are uncommon leap months.
];

const calendar = "chinese";

// Months can have up to 30 days.
const day = 30;

for (let monthCode of monthCodes) {
  let pmd = Temporal.PlainMonthDay.from({calendar, monthCode, day});
  assert.sameValue(pmd.monthCode, monthCode);
  assert.sameValue(pmd.day, day);

  let constrain = Temporal.PlainMonthDay.from({calendar, monthCode, day: day + 1}, {overflow: "constrain"});
  assert.sameValue(constrain.monthCode, monthCode);
  assert.sameValue(constrain.day, day);
  assertSameISOFields(constrain, pmd);

  assert.throws(RangeError, () => {
    Temporal.PlainMonthDay.from({calendar, monthCode, day: day + 1}, {overflow: "reject"});
  });
}

