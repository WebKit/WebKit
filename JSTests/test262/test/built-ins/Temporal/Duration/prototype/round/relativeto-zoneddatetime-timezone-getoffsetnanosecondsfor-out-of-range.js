// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: RangeError thrown if time zone reports an offset that is out of range
features: [Temporal]
includes: [temporalHelpers.js]
---*/

[-86400_000_000_001, 86400_000_000_001].forEach((wrongOffset) => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);
  const duration = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 987, 654, 321);
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, timeZone);
  assert.throws(RangeError, () => duration.round({ smallestUnit: "seconds", relativeTo: datetime }));
});
