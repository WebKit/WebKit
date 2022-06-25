// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: years value for largestUnit option
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainYearMonth(2000, 5);
const later = new Temporal.PlainYearMonth(2001, 6);

TemporalHelpers.assertDuration(later.since(earlier, { largestUnit: "years" }),
  1, 1, 0, 0, 0, 0, 0, 0, 0, 0, "largestUnit is years (pos)");
TemporalHelpers.assertDuration(earlier.since(later, { largestUnit: "years" }),
  -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, "largestUnit is years (neg)");
