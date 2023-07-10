// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.getcalendar
description: The "getCalendar" property of Temporal.PlainYearMonth.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainYearMonth.prototype.getCalendar,
  "function",
  "`typeof PlainYearMonth.prototype.getCalendar` is `function`"
);

verifyProperty(Temporal.PlainYearMonth.prototype, "getCalendar", {
  writable: true,
  enumerable: false,
  configurable: true,
});
