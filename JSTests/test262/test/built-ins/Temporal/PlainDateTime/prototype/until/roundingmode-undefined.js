// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: Fallback value for roundingMode option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 0, 0, 0);
const later = new Temporal.PlainDateTime(2000, 5, 3, 13, 35, 57, 123, 987, 500);

const explicit1 = earlier.until(later, { smallestUnit: "microsecond", roundingMode: undefined });
TemporalHelpers.assertDuration(explicit1, 0, 0, 0, 1, 1, 1, 1, 123, 987, 0, "default roundingMode is trunc");
const implicit1 = earlier.until(later, { smallestUnit: "microsecond" });
TemporalHelpers.assertDuration(implicit1, 0, 0, 0, 1, 1, 1, 1, 123, 987, 0, "default roundingMode is trunc");

const explicit2 = earlier.until(later, { smallestUnit: "millisecond", roundingMode: undefined });
TemporalHelpers.assertDuration(explicit2, 0, 0, 0, 1, 1, 1, 1, 123, 0, 0, "default roundingMode is trunc");
const implicit2 = earlier.until(later, { smallestUnit: "millisecond" });
TemporalHelpers.assertDuration(implicit2, 0, 0, 0, 1, 1, 1, 1, 123, 0, 0, "default roundingMode is trunc");

const explicit3 = earlier.until(later, { smallestUnit: "second", roundingMode: undefined });
TemporalHelpers.assertDuration(explicit3, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, "default roundingMode is trunc");
const implicit3 = earlier.until(later, { smallestUnit: "second" });
TemporalHelpers.assertDuration(implicit3, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, "default roundingMode is trunc");
