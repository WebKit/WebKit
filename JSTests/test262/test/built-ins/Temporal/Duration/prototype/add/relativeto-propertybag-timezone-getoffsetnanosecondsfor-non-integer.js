// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: RangeError thrown if time zone reports an offset that is not an integer number of nanoseconds
features: [Temporal]
includes: [temporalHelpers.js]
---*/

[3600_000_000_000.5, NaN, -Infinity, Infinity].forEach((wrongOffset) => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);
  const duration = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 987, 654, 321);
  const other = new Temporal.Duration(2);
  assert.throws(RangeError, () => duration.add(other, { relativeTo: { year: 2000, month: 5, day: 2, hour: 12, timeZone } }));
});
