// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: RangeError thrown if time zone reports an offset that is not an integer number of nanoseconds
features: [Temporal]
includes: [temporalHelpers.js]
---*/

[3600_000_000_000.5, NaN, -Infinity, Infinity].forEach((wrongOffset) => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);
  const date = new Temporal.PlainDate(2000, 5, 2);
  const plainTime = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);
  assert.throws(RangeError, () => date.toZonedDateTime({ plainTime, timeZone }));
});
