// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: The "getOffsetStringFor" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.getOffsetStringFor,
  "function",
  "`typeof TimeZone.prototype.getOffsetStringFor` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "getOffsetStringFor", {
  writable: true,
  enumerable: false,
  configurable: true,
});
