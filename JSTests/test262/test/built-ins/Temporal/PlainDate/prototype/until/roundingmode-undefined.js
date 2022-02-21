// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: Fallback value for roundingMode option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainDate(2000, 1, 1);

const later1 = new Temporal.PlainDate(2005, 2, 20);
const explicit1 = earlier.until(later1, { smallestUnit: "year", roundingMode: undefined });
TemporalHelpers.assertDuration(explicit1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, "default roundingMode is trunc");
const implicit1 = earlier.until(later1, { smallestUnit: "year" });
TemporalHelpers.assertDuration(implicit1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, "default roundingMode is trunc");

const later2 = new Temporal.PlainDate(2005, 12, 15);
const explicit2 = earlier.until(later2, { smallestUnit: "year", roundingMode: undefined });
TemporalHelpers.assertDuration(explicit2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, "default roundingMode is trunc");
const implicit2 = earlier.until(later2, { smallestUnit: "year" });
TemporalHelpers.assertDuration(implicit2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, "default roundingMode is trunc");
