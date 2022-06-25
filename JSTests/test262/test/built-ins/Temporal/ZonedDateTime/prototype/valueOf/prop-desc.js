// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.valueof
description: The "valueOf" property of Temporal.ZonedDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.ZonedDateTime.prototype.valueOf,
  "function",
  "`typeof ZonedDateTime.prototype.valueOf` is `function`"
);

verifyProperty(Temporal.ZonedDateTime.prototype, "valueOf", {
  writable: true,
  enumerable: false,
  configurable: true,
});
