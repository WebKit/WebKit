// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthsinyear
description: The "monthsInYear" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.monthsInYear,
  "function",
  "`typeof Calendar.prototype.monthsInYear` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "monthsInYear", {
  writable: true,
  enumerable: false,
  configurable: true,
});
