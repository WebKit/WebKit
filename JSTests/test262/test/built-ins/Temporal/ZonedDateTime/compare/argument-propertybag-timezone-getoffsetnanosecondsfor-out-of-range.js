// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: RangeError thrown if time zone reports an offset that is out of range
features: [Temporal]
includes: [temporalHelpers.js]
---*/

[-86400_000_000_001, 86400_000_000_001].forEach((wrongOffset) => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, timeZone);

  assert.throws(RangeError, () => Temporal.ZonedDateTime.compare({ year: 2000, month: 5, day: 2, hour: 12, timeZone }, datetime));
  assert.throws(RangeError, () => Temporal.ZonedDateTime.compare(datetime, { year: 2000, month: 5, day: 2, hour: 12, timeZone }));
});
