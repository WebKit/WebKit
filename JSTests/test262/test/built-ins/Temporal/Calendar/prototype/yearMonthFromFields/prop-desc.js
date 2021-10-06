// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: The "yearMonthFromFields" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.yearMonthFromFields,
  "function",
  "`typeof Calendar.prototype.yearMonthFromFields` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "yearMonthFromFields", {
  writable: true,
  enumerable: false,
  configurable: true,
});
