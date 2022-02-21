// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getnexttransition
description: The "getNextTransition" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.getNextTransition,
  "function",
  "`typeof TimeZone.prototype.getNextTransition` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "getNextTransition", {
  writable: true,
  enumerable: false,
  configurable: true,
});
