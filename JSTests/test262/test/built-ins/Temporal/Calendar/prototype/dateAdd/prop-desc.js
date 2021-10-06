// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: The "dateAdd" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.dateAdd,
  "function",
  "`typeof Calendar.prototype.dateAdd` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "dateAdd", {
  writable: true,
  enumerable: false,
  configurable: true,
});
