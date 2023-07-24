// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.getcalendar
description: The "getCalendar" property of Temporal.ZonedDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.ZonedDateTime.prototype.getCalendar,
  "function",
  "`typeof ZonedDateTime.prototype.getCalendar` is `function`"
);

verifyProperty(Temporal.ZonedDateTime.prototype, "getCalendar", {
  writable: true,
  enumerable: false,
  configurable: true,
});
