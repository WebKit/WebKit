// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.equals
description: The "equals" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.equals,
  "function",
  "`typeof TimeZone.prototype.equals` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "equals", {
  writable: true,
  enumerable: false,
  configurable: true,
});
