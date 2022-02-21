// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.tostring
description: The "toString" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.toString,
  "function",
  "`typeof Calendar.prototype.toString` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "toString", {
  writable: true,
  enumerable: false,
  configurable: true,
});
