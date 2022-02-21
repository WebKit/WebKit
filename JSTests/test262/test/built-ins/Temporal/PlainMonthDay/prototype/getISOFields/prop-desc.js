// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.getisofields
description: The "getISOFields" property of Temporal.PlainMonthDay.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainMonthDay.prototype.getISOFields,
  "function",
  "`typeof PlainMonthDay.prototype.getISOFields` is `function`"
);

verifyProperty(Temporal.PlainMonthDay.prototype, "getISOFields", {
  writable: true,
  enumerable: false,
  configurable: true,
});
