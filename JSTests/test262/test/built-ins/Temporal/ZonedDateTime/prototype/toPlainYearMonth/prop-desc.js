// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainyearmonth
description: The "toPlainYearMonth" property of Temporal.ZonedDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.ZonedDateTime.prototype.toPlainYearMonth,
  "function",
  "`typeof ZonedDateTime.prototype.toPlainYearMonth` is `function`"
);

verifyProperty(Temporal.ZonedDateTime.prototype, "toPlainYearMonth", {
  writable: true,
  enumerable: false,
  configurable: true,
});
