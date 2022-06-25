// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: relativeTo with years.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const oneYear = new Temporal.Duration(1);
const days365 = new Temporal.Duration(0, 0, 0, 365);
TemporalHelpers.assertDuration(oneYear.add(days365, { relativeTo: Temporal.PlainDate.from("2016-01-01") }),
  2, 0, 0, 0, 0, 0, 0, 0, 0, 0, "non-leap year");
TemporalHelpers.assertDuration(oneYear.add(days365, { relativeTo: Temporal.PlainDate.from("2015-01-01") }),
  1, 11, 0, 30, 0, 0, 0, 0, 0, 0, "leap year");
