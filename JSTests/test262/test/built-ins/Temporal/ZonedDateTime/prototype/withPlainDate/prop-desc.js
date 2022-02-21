// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: The "withPlainDate" property of Temporal.ZonedDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.ZonedDateTime.prototype.withPlainDate,
  "function",
  "`typeof ZonedDateTime.prototype.withPlainDate` is `function`"
);

verifyProperty(Temporal.ZonedDateTime.prototype, "withPlainDate", {
  writable: true,
  enumerable: false,
  configurable: true,
});
