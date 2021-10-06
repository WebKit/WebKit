// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: The "getPlainDateTimeFor" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.getPlainDateTimeFor,
  "function",
  "`typeof TimeZone.prototype.getPlainDateTimeFor` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "getPlainDateTimeFor", {
  writable: true,
  enumerable: false,
  configurable: true,
});
