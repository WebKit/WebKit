// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: PlainDate-like string argument is acceptable
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const dt = new Temporal.PlainDateTime(1995, 12, 7, 3, 24, 30);

TemporalHelpers.assertPlainDateTime(
  dt.withPlainDate("2018-09-15"),
  2018, 9, "M09", 15, 3, 24, 30, 0, 0, 0,
  "PlainDate-like string argument works"
);
