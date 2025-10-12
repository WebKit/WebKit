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

// Input which has to skip the first two candidates when starting the search from
// December 31, 1972.
//
// Note: December 31, 1972 is year 1689 in the coptic calendar.
//
// 1. First candidate 1689-M13-05 ("1973-09-10[u-ca=coptic]") is after December 31, 1972.
// 2. Second candidate 1688-M13-05 ("1972-09-10[u-ca=coptic]") is before December 31, 1972,
//    but day doesn't match.
// 3. Third candidate 1687-M13-06 ("1971-09-11[u-ca=coptic]") is a full match.
{
  let pmd = Temporal.PlainMonthDay.from({
    calendar: "coptic",
    monthCode: "M13",
    day: 7,
  });
  assert.sameValue(pmd.monthCode, "M13");
  assert.sameValue(pmd.day, 6);

  let fields = ISOFields(pmd);
  assert.sameValue(fields.calendar, "coptic");
  assert.sameValue(fields.isoYear, 1971);
  assert.sameValue(fields.isoMonth, 9);
  assert.sameValue(fields.isoDay, 11);
}

