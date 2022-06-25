// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.from
description: Non-ASCII minus sign is acceptable
features: [Temporal]
includes: [temporalHelpers.js]
---*/

TemporalHelpers.assertPlainDateTime(
  Temporal.PlainDateTime.from("1976-11-18T15:23:30.12\u221202:00"),
  1976, 11, "M11", 18, 15, 23, 30, 120, 0, 0,
  "variant minus sign (offset)"
);

TemporalHelpers.assertPlainDateTime(
  Temporal.PlainDateTime.from("\u2212009999-11-18T15:23:30.12"),
  -9999, 11, "M11", 18, 15, 23, 30, 120, 0, 0,
  "variant minus sign (leading minus)"
);
