// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: The "weekOfYear" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.weekOfYear,
  "function",
  "`typeof Calendar.prototype.weekOfYear` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "weekOfYear", {
  writable: true,
  enumerable: false,
  configurable: true,
});
