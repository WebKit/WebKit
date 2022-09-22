// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Empty or a function object may be used as options
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const result1 = instance.yearMonthFromFields({ year: 2000, monthCode: "M05" }, {});
TemporalHelpers.assertPlainYearMonth(
  result1, 2000, 5, "M05",
  "options may be an empty plain object"
);

const result2 = instance.yearMonthFromFields({ year: 2000, monthCode: "M05" }, () => {});
TemporalHelpers.assertPlainYearMonth(
  result2, 2000, 5, "M05",
  "options may be a function object"
);
