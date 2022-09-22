// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Empty or a function object may be used as options
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const result1 = instance.dateUntil(new Temporal.PlainDate(1976, 11, 18), new Temporal.PlainDate(1984, 5, 31), {});
TemporalHelpers.assertDuration(
  result1, 0, 0, 0, 2751, 0, 0, 0, 0, 0, 0,
  "options may be an empty plain object"
);

const result2 = instance.dateUntil(new Temporal.PlainDate(1976, 11, 18), new Temporal.PlainDate(1984, 5, 31), () => {});
TemporalHelpers.assertDuration(
  result2, 0, 0, 0, 2751, 0, 0, 0, 0, 0, 0,
  "options may be a function object"
);
