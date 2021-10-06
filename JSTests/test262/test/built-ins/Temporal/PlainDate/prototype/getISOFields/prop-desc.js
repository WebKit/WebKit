// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.getisofields
description: The "getISOFields" property of Temporal.PlainDate.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainDate.prototype.getISOFields,
  "function",
  "`typeof PlainDate.prototype.getISOFields` is `function`"
);

verifyProperty(Temporal.PlainDate.prototype, "getISOFields", {
  writable: true,
  enumerable: false,
  configurable: true,
});
