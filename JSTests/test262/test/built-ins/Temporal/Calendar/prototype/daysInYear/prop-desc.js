// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.daysinyear
description: The "daysInYear" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.daysInYear,
  "function",
  "`typeof Calendar.prototype.daysInYear` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "daysInYear", {
  writable: true,
  enumerable: false,
  configurable: true,
});
