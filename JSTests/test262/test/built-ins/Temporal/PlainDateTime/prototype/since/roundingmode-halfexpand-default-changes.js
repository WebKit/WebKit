// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.since
description: A different default for largest unit will be used if smallest unit is larger than "days"
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainDateTime(2019, 1, 8, 8, 22, 36, 123, 456, 789);
const later = new Temporal.PlainDateTime(2021, 9, 7, 12, 39, 40, 987, 654, 321);

TemporalHelpers.assertDuration(
  later.since(earlier, { smallestUnit: "years", roundingMode: "halfExpand" }),
  3, 0, 0, 0, 0, 0, 0, 0, 0,0,
  "assumes a different default for largestUnit if smallestUnit is larger than days (smallest unit = years)"
);

TemporalHelpers.assertDuration(
  later.since(earlier, { smallestUnit: "months", roundingMode: "halfExpand" }),
  0, 32, 0, 0, 0, 0, 0, 0, 0, 0,
  "assumes a different default for largestUnit if smallestUnit is larger than days (smallest unit = months)"
);

TemporalHelpers.assertDuration(
  later.since(earlier, { smallestUnit: "weeks", roundingMode: "halfExpand" }),
  0, 0, 139, 0, 0, 0, 0, 0, 0, 0,
  "assumes a different default for largestUnit if smallestUnit is larger than days (smallest unit = weeks)"
);
