// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: relativeTo with years.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const oneYear = new Temporal.Duration(1);
const days365 = new Temporal.Duration(0, 0, 0, 365);
TemporalHelpers.assertDuration(oneYear.subtract(days365, { relativeTo: Temporal.PlainDate.from("2017-01-01") }),
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "non-leap year");
TemporalHelpers.assertDuration(oneYear.subtract(days365, { relativeTo: Temporal.PlainDate.from("2016-01-01") }),
  0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "leap year");
