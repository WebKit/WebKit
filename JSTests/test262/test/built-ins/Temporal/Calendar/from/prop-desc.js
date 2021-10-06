// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: The "from" property of Temporal.Calendar
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.from,
  "function",
  "`typeof Calendar.from` is `function`"
);

verifyProperty(Temporal.Calendar, "from", {
  writable: true,
  enumerable: false,
  configurable: true,
});
