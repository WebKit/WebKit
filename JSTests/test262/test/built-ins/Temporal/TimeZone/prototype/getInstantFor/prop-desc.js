// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: The "getInstantFor" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.getInstantFor,
  "function",
  "`typeof TimeZone.prototype.getInstantFor` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "getInstantFor", {
  writable: true,
  enumerable: false,
  configurable: true,
});
