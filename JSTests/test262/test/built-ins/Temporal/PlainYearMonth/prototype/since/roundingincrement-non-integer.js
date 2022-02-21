// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Rounding for roundingIncrement option
info: |
    sec-temporal-totemporalroundingincrement step 7:
      7. Set _increment_ to floor(ℝ(_increment_)).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainYearMonth(2000, 5);
const later = new Temporal.PlainYearMonth(2000, 10);
const result = later.since(earlier, { roundingIncrement: 2.5 });
TemporalHelpers.assertDuration(result, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, "roundingIncrement 2.5 floors to 2");
