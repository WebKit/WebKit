// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: The "mergeFields" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.mergeFields,
  "function",
  "`typeof Calendar.prototype.mergeFields` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "mergeFields", {
  writable: true,
  enumerable: false,
  configurable: true,
});
