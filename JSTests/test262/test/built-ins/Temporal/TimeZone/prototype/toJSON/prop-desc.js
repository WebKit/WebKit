// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.tojson
description: The "toJSON" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.toJSON,
  "function",
  "`typeof TimeZone.prototype.toJSON` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "toJSON", {
  writable: true,
  enumerable: false,
  configurable: true,
});
