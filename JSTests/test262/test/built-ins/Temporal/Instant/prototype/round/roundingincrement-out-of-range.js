// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.round
description: RangeError thrown when roundingIncrement option out of range
info: |
    sec-temporal-totemporalroundingincrement step 6:
      6. If _increment_ < 1 or _increment_ > _maximum_, throw a *RangeError* exception.
features: [Temporal]
---*/

const instant = new Temporal.Instant(1_000_000_000_000_000_005n);
assert.throws(RangeError, () => instant.round({ smallestUnit: "nanoseconds", roundingIncrement: -Infinity }));
assert.throws(RangeError, () => instant.round({ smallestUnit: "nanoseconds", roundingIncrement: -1 }));
assert.throws(RangeError, () => instant.round({ smallestUnit: "nanoseconds", roundingIncrement: 0 }));
assert.throws(RangeError, () => instant.round({ smallestUnit: "nanoseconds", roundingIncrement: Infinity }));
