// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.with
description: The "with" property of Temporal.Duration.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Duration.prototype.with,
  "function",
  "`typeof Duration.prototype.with` is `function`"
);

verifyProperty(Temporal.Duration.prototype, "with", {
  writable: true,
  enumerable: false,
  configurable: true,
});
