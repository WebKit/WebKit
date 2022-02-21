// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.tojson
description: The "toJSON" property of Temporal.Calendar.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Calendar.prototype.toJSON,
  "function",
  "`typeof Calendar.prototype.toJSON` is `function`"
);

verifyProperty(Temporal.Calendar.prototype, "toJSON", {
  writable: true,
  enumerable: false,
  configurable: true,
});
