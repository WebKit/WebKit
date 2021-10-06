// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.month
description: The "month" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.month,
  "function",
  "`typeof Calendar.prototype.month` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "month", {
  writable: true,
  enumerable: false,
  configurable: true,
});
