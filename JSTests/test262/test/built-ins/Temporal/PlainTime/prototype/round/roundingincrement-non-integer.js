// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.round
description: Rounding for roundingIncrement option
info: |
    sec-temporal-totemporalroundingincrement step 7:
      7. Set _increment_ to floor(ℝ(_increment_)).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const time = new Temporal.PlainTime(12, 34, 56, 0, 0, 5);
const result = time.round({ smallestUnit: "nanosecond", roundingIncrement: 2.5 });
TemporalHelpers.assertPlainTime(result, 12, 34, 56, 0, 0, 6, "roundingIncrement 2.5 floors to 2");
