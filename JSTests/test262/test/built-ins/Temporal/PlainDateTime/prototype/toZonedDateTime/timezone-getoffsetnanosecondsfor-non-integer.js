// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tozoneddatetime
description: RangeError thrown if time zone reports an offset that is not an integer number of nanoseconds
features: [Temporal]
includes: [temporalHelpers.js]
---*/

[3600_000_000_000.5, NaN, -Infinity, Infinity].forEach((wrongOffset) => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);
  const datetime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321);
  timeZone.getPossibleInstantsFor = function () {
    return [];
  };
  assert.throws(RangeError, () => datetime.toZonedDateTime(timeZone));
});
