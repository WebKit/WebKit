// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: An ISO 8601 string can be converted to a calendar ID in Calendar
features: [Temporal]
---*/

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
  const timeZone = new Temporal.TimeZone("UTC");
const arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
  const result = Temporal.ZonedDateTime.from(arg);
  assert.sameValue(result.calendarId, "iso8601", `Calendar created from string "${calendar}"`);
  assert.sameValue(result.getISOFields().calendar, "iso8601", "calendar slot stores a string");
}
