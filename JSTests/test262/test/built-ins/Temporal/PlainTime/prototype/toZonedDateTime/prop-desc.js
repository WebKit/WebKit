// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: The "toZonedDateTime" property of Temporal.PlainTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainTime.prototype.toZonedDateTime,
  "function",
  "`typeof PlainTime.prototype.toZonedDateTime` is `function`"
);

verifyProperty(Temporal.PlainTime.prototype, "toZonedDateTime", {
  writable: true,
  enumerable: false,
  configurable: true,
});
