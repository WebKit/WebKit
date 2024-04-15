// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
description: An ISO 8601 string can be converted to a calendar ID in Calendar
features: [Temporal]
---*/

for (const arg of [
  "2020-01-01",
  "2020-01-01[u-ca=iso8601]",
  "2020-01-01T00:00:00.000000000",
  "2020-01-01T00:00:00.000000000[u-ca=iso8601]",
  "01-01",
  "01-01[u-ca=iso8601]",
  "2020-01",
  "2020-01[u-ca=iso8601]",
]) {
  const result = Temporal.Now.plainDateTime(arg);
  assert.sameValue(result.getISOFields().calendar, "iso8601", `Calendar created from string "${arg}"`);
}
