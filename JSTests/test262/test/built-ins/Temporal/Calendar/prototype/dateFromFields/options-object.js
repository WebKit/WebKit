// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.datefromfields
description: Empty or a function object may be used as options
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const result1 = instance.dateFromFields({ year: 1976, month: 11, day: 18 }, {});
TemporalHelpers.assertPlainDate(
  result1, 1976, 11, "M11", 18,
  "options may be an empty plain object"
);

const result2 = instance.dateFromFields({ year: 1976, month: 11, day: 18 }, () => {});
TemporalHelpers.assertPlainDate(
  result2, 1976, 11, "M11", 18,
  "options may be a function object"
);
