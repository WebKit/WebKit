// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.toplainmonthday
description: The "toPlainMonthDay" property of Temporal.PlainDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainDateTime.prototype.toPlainMonthDay,
  "function",
  "`typeof PlainDateTime.prototype.toPlainMonthDay` is `function`"
);

verifyProperty(Temporal.PlainDateTime.prototype, "toPlainMonthDay", {
  writable: true,
  enumerable: false,
  configurable: true,
});
