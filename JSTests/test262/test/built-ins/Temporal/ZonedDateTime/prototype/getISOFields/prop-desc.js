// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.getisofields
description: The "getISOFields" property of Temporal.ZonedDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.ZonedDateTime.prototype.getISOFields,
  "function",
  "`typeof ZonedDateTime.prototype.getISOFields` is `function`"
);

verifyProperty(Temporal.ZonedDateTime.prototype, "getISOFields", {
  writable: true,
  enumerable: false,
  configurable: true,
});
