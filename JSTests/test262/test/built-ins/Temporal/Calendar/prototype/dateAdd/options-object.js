// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Empty or a function object may be used as options
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const result1 = instance.dateAdd(new Temporal.PlainDate(1976, 11, 18), new Temporal.Duration(1), {});
TemporalHelpers.assertPlainDate(
  result1, 1977, 11, "M11", 18,
  "options may be an empty plain object"
);

const result2 = instance.dateAdd(new Temporal.PlainDate(1976, 11, 18), new Temporal.Duration(1), () => {});
TemporalHelpers.assertPlainDate(
  result2, 1977, 11, "M11", 18,
  "options may be a function object"
);
