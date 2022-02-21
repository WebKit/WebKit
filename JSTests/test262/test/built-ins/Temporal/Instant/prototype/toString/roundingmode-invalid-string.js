// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tostring
description: RangeError thrown when roundingMode option not one of the allowed string values
features: [Temporal]
---*/

const instant = new Temporal.Instant(1_000_000_000_123_987_500n);
assert.throws(RangeError, () => instant.toString({ smallestUnit: "microsecond", roundingMode: "other string" }));
