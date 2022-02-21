// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar
description: The "Calendar" property of Temporal
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar,
  "function",
  "`typeof Calendar` is `function`"
);

verifyProperty(Temporal, "Calendar", {
  writable: true,
  enumerable: false,
  configurable: true,
});
