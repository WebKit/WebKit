// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.getplaindatetimefor
description: Checking limits of representable PlainDateTime
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const min = new Temporal.Instant(-8_640_000_000_000_000_000_000n);
const offsetMin = new Temporal.TimeZone("-23:59");
const max = new Temporal.Instant(8_640_000_000_000_000_000_000n);
const offsetMax = new Temporal.TimeZone("+23:59");

TemporalHelpers.assertPlainDateTime(
  offsetMin.getPlainDateTimeFor(min, "iso8601"),
  -271821, 4, "M04", 19, 0, 1, 0, 0, 0, 0,
  "converting from Instant (negative case)"
);

TemporalHelpers.assertPlainDateTime(
  offsetMax.getPlainDateTimeFor(max, "iso8601"),
  275760, 9, "M09", 13, 23, 59, 0, 0, 0, 0,
  "converting from Instant (positive case)"
);
