// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.from
description: Time separator in string argument can vary
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const expected = [1976, 11, "M11", 18, 15, 23, 0, 0, 0, 0];

TemporalHelpers.assertPlainDateTime(
  Temporal.PlainDateTime.from("1976-11-18T15:23"),
  ...expected,
  "variant time separators (uppercase T)"
);

TemporalHelpers.assertPlainDateTime(
  Temporal.PlainDateTime.from("1976-11-18t15:23"),
  ...expected,
  "variant time separators (lowercase T)"
);

TemporalHelpers.assertPlainDateTime(
  Temporal.PlainDateTime.from("1976-11-18 15:23"),
  ...expected,
  "variant time separators (space between date and time)"
);
