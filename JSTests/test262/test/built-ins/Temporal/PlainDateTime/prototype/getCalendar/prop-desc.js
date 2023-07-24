// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.getcalendar
description: The "getCalendar" property of Temporal.PlainDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainDateTime.prototype.getCalendar,
  "function",
  "`typeof PlainDateTime.prototype.getCalendar` is `function`"
);

verifyProperty(Temporal.PlainDateTime.prototype, "getCalendar", {
  writable: true,
  enumerable: false,
  configurable: true,
});
