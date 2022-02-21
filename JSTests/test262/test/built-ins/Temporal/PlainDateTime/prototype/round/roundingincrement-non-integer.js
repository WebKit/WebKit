// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.round
description: Rounding for roundingIncrement option
info: |
    sec-temporal-totemporalroundingincrement step 7:
      7. Set _increment_ to floor(ℝ(_increment_)).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const datetime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 0, 0, 5);
const result = datetime.round({ smallestUnit: "nanosecond", roundingIncrement: 2.5 });
TemporalHelpers.assertPlainDateTime(result, 2000, 5, "M05", 2, 12, 34, 56, 0, 0, 6, "roundingIncrement 2.5 floors to 2");
