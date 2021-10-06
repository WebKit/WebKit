// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getprevioustransition
description: The "getPreviousTransition" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.getPreviousTransition,
  "function",
  "`typeof TimeZone.prototype.getPreviousTransition` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "getPreviousTransition", {
  writable: true,
  enumerable: false,
  configurable: true,
});
