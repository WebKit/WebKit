// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.tojson
description: The "toJSON" property of Temporal.PlainMonthDay.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainMonthDay.prototype.toJSON,
  "function",
  "`typeof PlainMonthDay.prototype.toJSON` is `function`"
);

verifyProperty(Temporal.PlainMonthDay.prototype, "toJSON", {
  writable: true,
  enumerable: false,
  configurable: true,
});
