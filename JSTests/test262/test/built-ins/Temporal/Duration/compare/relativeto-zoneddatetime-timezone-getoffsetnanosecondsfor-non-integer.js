// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: RangeError thrown if time zone reports an offset that is not an integer number of nanoseconds
features: [Temporal]
includes: [temporalHelpers.js]
---*/

[3600_000_000_000.5, NaN].forEach((wrongOffset) => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);
  const relativeTo = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, timeZone);
  const duration1 = new Temporal.Duration(1);
  const duration2 = new Temporal.Duration(1, 1);
  assert.throws(RangeError, () => Temporal.Duration.compare(duration1, duration2, { relativeTo }));
});
