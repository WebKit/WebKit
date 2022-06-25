// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: relativeTo with months.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const oneMonth = new Temporal.Duration(0, 1);
const days30 = new Temporal.Duration(0, 0, 0, 30);
TemporalHelpers.assertDuration(oneMonth.subtract(days30, { relativeTo: Temporal.PlainDate.from('2018-02-01') }),
  0, 0, 0, -2, 0, 0, 0, 0, 0, 0, "February");
TemporalHelpers.assertDuration(oneMonth.subtract(days30, { relativeTo: Temporal.PlainDate.from('2018-03-01') }),
  0, 0, 0, 1, 0, 0, 0, 0, 0, 0, "March");
TemporalHelpers.assertDuration(oneMonth.subtract(days30, { relativeTo: Temporal.PlainDate.from('2018-04-01') }),
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "April");
