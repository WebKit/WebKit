// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetime
description: The "toZonedDateTime" property of Temporal.Instant.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Instant.prototype.toZonedDateTime,
  "function",
  "`typeof Instant.prototype.toZonedDateTime` is `function`"
);

verifyProperty(Temporal.Instant.prototype, "toZonedDateTime", {
  writable: true,
  enumerable: false,
  configurable: true,
});
