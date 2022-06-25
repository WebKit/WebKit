// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: PlainDate object is acceptable
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dt = new Temporal.PlainDateTime(1995, 12, 7, 3, 24, 30);
const date = new Temporal.PlainDate(2020, 1, 23);

TemporalHelpers.assertPlainDateTime(
  dt.withPlainDate(date),
  2020, 1, "M01", 23, 3, 24, 30, 0, 0, 0,
  "PlainDate argument works"
);
