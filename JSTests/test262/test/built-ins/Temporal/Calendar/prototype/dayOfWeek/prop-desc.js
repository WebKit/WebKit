// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dayofweek
description: The "dayOfWeek" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.dayOfWeek,
  "function",
  "`typeof Calendar.prototype.dayOfWeek` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "dayOfWeek", {
  writable: true,
  enumerable: false,
  configurable: true,
});
