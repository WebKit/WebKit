// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.getisofields
description: The "getISOFields" property of Temporal.PlainYearMonth.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainYearMonth.prototype.getISOFields,
  "function",
  "`typeof PlainYearMonth.prototype.getISOFields` is `function`"
);

verifyProperty(Temporal.PlainYearMonth.prototype, "getISOFields", {
  writable: true,
  enumerable: false,
  configurable: true,
});
