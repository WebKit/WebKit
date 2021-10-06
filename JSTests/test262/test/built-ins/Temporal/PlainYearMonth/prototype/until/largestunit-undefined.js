// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: Fallback value for largestUnit option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainYearMonth(2000, 5);
const later = new Temporal.PlainYearMonth(2001, 6);

const explicit = earlier.until(later, { largestUnit: undefined });
TemporalHelpers.assertDuration(explicit, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, "default largestUnit is year");
const implicit = earlier.until(later, {});
TemporalHelpers.assertDuration(implicit, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, "default largestUnit is year");
