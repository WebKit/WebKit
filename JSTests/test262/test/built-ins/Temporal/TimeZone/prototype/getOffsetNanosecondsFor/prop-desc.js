// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetnanosecondsfor
description: The "getOffsetNanosecondsFor" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.getOffsetNanosecondsFor,
  "function",
  "`typeof TimeZone.prototype.getOffsetNanosecondsFor` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "getOffsetNanosecondsFor", {
  writable: true,
  enumerable: false,
  configurable: true,
});
