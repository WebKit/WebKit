// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.getisofields
description: The "getISOFields" property of Temporal.PlainDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainDateTime.prototype.getISOFields,
  "function",
  "`typeof PlainDateTime.prototype.getISOFields` is `function`"
);

verifyProperty(Temporal.PlainDateTime.prototype, "getISOFields", {
  writable: true,
  enumerable: false,
  configurable: true,
});
