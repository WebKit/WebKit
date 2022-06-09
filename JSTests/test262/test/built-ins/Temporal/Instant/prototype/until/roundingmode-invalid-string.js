// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.until
description: RangeError thrown when roundingMode option not one of the allowed string values
features: [Temporal]
---*/

const earlier = new Temporal.Instant(1_000_000_000_000_000_000n);
const later = new Temporal.Instant(1_000_090_061_123_987_500n);
assert.throws(RangeError, () => earlier.until(later, { smallestUnit: "microsecond", roundingMode: "other string" }));
