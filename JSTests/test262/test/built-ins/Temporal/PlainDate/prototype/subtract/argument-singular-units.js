// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.subtract
description: Ignores singular properties
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const date = Temporal.PlainDate.from("2019-11-18");
TemporalHelpers.assertPlainDate(date.subtract({ month: 1, days: 1 }),
  2019, 11, "M11", 17);
