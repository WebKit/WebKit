// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.getcalendar
description: The "getCalendar" property of Temporal.PlainDate.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainDate.prototype.getCalendar,
  "function",
  "`typeof PlainDate.prototype.getCalendar` is `function`"
);

verifyProperty(Temporal.PlainDate.prototype, "getCalendar", {
  writable: true,
  enumerable: false,
  configurable: true,
});
