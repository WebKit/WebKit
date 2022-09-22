// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: RangeError thrown if time zone reports an offset that is out of range
features: [Temporal]
includes: [temporalHelpers.js]
---*/

[-86400_000_000_000, 86400_000_000_000].forEach((wrongOffset) => {
  const timeZone = TemporalHelpers.specificOffsetTimeZone(wrongOffset);
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC");
  const properties = { year: 2004, month: 11, day: 9, hour: 11, minute: 33, second: 20, timeZone };
  timeZone.getPossibleInstantsFor = function () {
    return [];
  };
  assert.throws(RangeError, () => datetime.since(properties, { largestUnit: "days" }));
});
