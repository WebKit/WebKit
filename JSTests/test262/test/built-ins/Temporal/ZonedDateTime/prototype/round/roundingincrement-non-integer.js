// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: Rounding for roundingIncrement option
info: |
    sec-temporal-totemporalroundingincrement step 7:
      7. Set _increment_ to floor(ℝ(_increment_)).
features: [Temporal]
---*/

const datetime = new Temporal.ZonedDateTime(1_000_000_000_000_000_005n, "UTC");
const result = datetime.round({ smallestUnit: "nanosecond", roundingIncrement: 2.5 });
assert.sameValue(result.epochNanoseconds, 1_000_000_000_000_000_006n, "roundingIncrement 2.5 floors to 2");
