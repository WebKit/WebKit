// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: An ISO 8601 string can be converted to a calendar ID in Calendar
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

for (const calendar of [
  "2020-01-01",
  "2020-01-01[u-ca=iso8601]",
  "2020-01-01T00:00:00.000000000",
  "2020-01-01T00:00:00.000000000[u-ca=iso8601]",
  "01-01",
  "01-01[u-ca=iso8601]",
  "2020-01",
  "2020-01[u-ca=iso8601]",
]) {
  const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
  const result = instance.toZonedDateTime({ plainDate: arg, timeZone: "UTC" });
  assert.sameValue(result.epochNanoseconds, 217_168_496_987_654_321n, `Calendar created from string "${calendar}"`);
}
