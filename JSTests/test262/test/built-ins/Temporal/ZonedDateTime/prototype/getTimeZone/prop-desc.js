// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.gettimezone
description: The "getTimeZone" property of Temporal.ZonedDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.ZonedDateTime.prototype.getTimeZone,
  "function",
  "`typeof ZonedDateTime.prototype.getTimeZone` is `function`"
);

verifyProperty(Temporal.ZonedDateTime.prototype, "getTimeZone", {
  writable: true,
  enumerable: false,
  configurable: true,
});
