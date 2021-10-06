// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: The "from" property of Temporal.TimeZone
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.from,
  "function",
  "`typeof TimeZone.from` is `function`"
);

verifyProperty(Temporal.TimeZone, "from", {
  writable: true,
  enumerable: false,
  configurable: true,
});
