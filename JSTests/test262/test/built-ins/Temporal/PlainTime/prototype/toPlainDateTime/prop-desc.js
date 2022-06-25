// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.toplaindatetime
description: The "toPlainDateTime" property of Temporal.PlainTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainTime.prototype.toPlainDateTime,
  "function",
  "`typeof PlainTime.prototype.toPlainDateTime` is `function`"
);

verifyProperty(Temporal.PlainTime.prototype, "toPlainDateTime", {
  writable: true,
  enumerable: false,
  configurable: true,
});
