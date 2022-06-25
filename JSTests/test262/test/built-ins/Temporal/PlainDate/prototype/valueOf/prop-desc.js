// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.valueof
description: The "valueOf" property of Temporal.PlainDate.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainDate.prototype.valueOf,
  "function",
  "`typeof PlainDate.prototype.valueOf` is `function`"
);

verifyProperty(Temporal.PlainDate.prototype, "valueOf", {
  writable: true,
  enumerable: false,
  configurable: true,
});
