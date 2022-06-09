// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.round
description: Fallback value for roundingMode option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const time = new Temporal.PlainTime(12, 34, 56, 123, 987, 500);

const explicit1 = time.round({ smallestUnit: "microsecond", roundingMode: undefined });
TemporalHelpers.assertPlainTime(explicit1, 12, 34, 56, 123, 988, 0, "default roundingMode is halfExpand");
const implicit1 = time.round({ smallestUnit: "microsecond" });
TemporalHelpers.assertPlainTime(implicit1, 12, 34, 56, 123, 988, 0, "default roundingMode is halfExpand");

const explicit2 = time.round({ smallestUnit: "millisecond", roundingMode: undefined });
TemporalHelpers.assertPlainTime(explicit2, 12, 34, 56, 124, 0, 0, "default roundingMode is halfExpand");
const implicit2 = time.round({ smallestUnit: "millisecond" });
TemporalHelpers.assertPlainTime(implicit2, 12, 34, 56, 124, 0, 0, "default roundingMode is halfExpand");

const explicit3 = time.round({ smallestUnit: "second", roundingMode: undefined });
TemporalHelpers.assertPlainTime(explicit3, 12, 34, 56, 0, 0, 0, "default roundingMode is halfExpand");
const implicit3 = time.round({ smallestUnit: "second" });
TemporalHelpers.assertPlainTime(implicit3, 12, 34, 56, 0, 0, 0, "default roundingMode is halfExpand");
