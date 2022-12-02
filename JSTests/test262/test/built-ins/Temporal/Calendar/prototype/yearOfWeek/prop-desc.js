// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearofweek
description: The "yearOfWeek" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.yearOfWeek,
  "function",
  "`typeof Calendar.prototype.yearOfWeek` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "yearOfWeek", {
  writable: true,
  enumerable: false,
  configurable: true,
});
