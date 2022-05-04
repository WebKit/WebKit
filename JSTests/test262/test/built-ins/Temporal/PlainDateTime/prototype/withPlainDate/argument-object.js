// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: Plain object may be acceptable
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const dt = new Temporal.PlainDateTime(1995, 12, 7, 3, 24, 30);

TemporalHelpers.assertPlainDateTime(
  dt.withPlainDate({ year: 2000, month: 6, day: 1 }),
  2000, 6, "M06", 1, 3, 24, 30, 0, 0, 0,
  "plain object works"
);
