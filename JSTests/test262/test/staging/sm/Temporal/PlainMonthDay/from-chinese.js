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

// Non-leap month should find a result in years around 1972.
//
// Month -> ISO year
// 
// M01      1970
// M02      1972
// M03      1966
// M04      1970
// M05      1972
// M06      1971
// M07      1972
// M08      1971
// M09      1972
// M10      1972
// M11      1970
// M12      1972

const monthCodes = [
  "M01",
  "M02",
  "M03",
  "M04",
  "M05",
  "M06",
  "M07",
  "M08",
  "M09",
  "M10",
  "M11",
  "M12",
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

