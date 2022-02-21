// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.inleapyear
description: The "inLeapYear" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.inLeapYear,
  "function",
  "`typeof Calendar.prototype.inLeapYear` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "inLeapYear", {
  writable: true,
  enumerable: false,
  configurable: true,
});
