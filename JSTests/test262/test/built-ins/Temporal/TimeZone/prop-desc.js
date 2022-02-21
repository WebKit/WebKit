// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone
description: The "TimeZone" property of Temporal
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone,
  "function",
  "`typeof TimeZone` is `function`"
);

verifyProperty(Temporal, "TimeZone", {
  writable: true,
  enumerable: false,
  configurable: true,
});
