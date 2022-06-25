// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.compare
description: RangeError thrown if time zone reports an offset that is out of range
features: [Temporal]
includes: [temporalHelpers.js]
---*/

[-86400_000_000_001, 86400_000_000_001].forEach((wrongOffset) => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, timeZone);
  const time = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

  assert.throws(RangeError, () => Temporal.PlainTime.compare(datetime, time));
  assert.throws(RangeError, () => Temporal.PlainTime.compare(time, datetime));
});
