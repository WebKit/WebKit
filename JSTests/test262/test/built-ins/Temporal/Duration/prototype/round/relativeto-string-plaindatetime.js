// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: The relativeTo option accepts a PlainDateTime-like ISO 8601 string
features: [Temporal]
includes: [temporalHelpers.js]
---*/

['2000-01-01', '2000-01-01T00:00', '2000-01-01T00:00[u-ca=iso8601]'].forEach((relativeTo) => {
  const duration = new Temporal.Duration(0, 0, 0, 31);
  const result = duration.round({ largestUnit: "months", relativeTo });
  TemporalHelpers.assertDuration(result, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0);
});
