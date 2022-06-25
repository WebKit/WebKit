// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: The "dateUntil" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.dateUntil,
  "function",
  "`typeof Calendar.prototype.dateUntil` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "dateUntil", {
  writable: true,
  enumerable: false,
  configurable: true,
});
