// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getpossibleinstantsfor
description: The "getPossibleInstantsFor" property of Temporal.TimeZone.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.TimeZone.prototype.getPossibleInstantsFor,
  "function",
  "`typeof TimeZone.prototype.getPossibleInstantsFor` is `function`"
);

verifyProperty(Temporal.TimeZone.prototype, "getPossibleInstantsFor", {
  writable: true,
  enumerable: false,
  configurable: true,
});
