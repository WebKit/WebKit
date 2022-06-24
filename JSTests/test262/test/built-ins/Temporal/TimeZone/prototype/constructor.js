// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.constructor
description: Test for Temporal.TimeZone.prototype.constructor.
info: The initial value of Temporal.TimeZone.prototype.constructor is %Temporal.TimeZone%.
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.TimeZone.prototype, "constructor", {
  value: Temporal.TimeZone,
  writable: true,
  enumerable: false,
  configurable: true,
});
