// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: Throw RangeError for an out-of-range, conflicting, or ill-formed monthCode
features: [Temporal]
---*/

["m1", "M1", "m01"].forEach((monthCode) => {
  assert.throws(RangeError, () => Temporal.PlainYearMonth.from({ year: 2021, monthCode }),
    `monthCode '${monthCode}' is not well-formed`);
});

assert.throws(RangeError, () => Temporal.PlainYearMonth.from({ year: 2021, month: 12, monthCode: "M11" }),
     "monthCode and month conflict");

["M00", "M19", "M99", "M13"].forEach((monthCode) => {
  assert.throws(RangeError, () => Temporal.PlainYearMonth.from({ year: 2021, monthCode }),
    `monthCode '${monthCode}' is not valid for year 2021`);
});
