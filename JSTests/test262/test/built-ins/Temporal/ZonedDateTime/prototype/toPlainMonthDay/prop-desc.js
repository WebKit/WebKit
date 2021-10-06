// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainmonthday
description: The "toPlainMonthDay" property of Temporal.ZonedDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.ZonedDateTime.prototype.toPlainMonthDay,
  "function",
  "`typeof ZonedDateTime.prototype.toPlainMonthDay` is `function`"
);

verifyProperty(Temporal.ZonedDateTime.prototype, "toPlainMonthDay", {
  writable: true,
  enumerable: false,
  configurable: true,
});
