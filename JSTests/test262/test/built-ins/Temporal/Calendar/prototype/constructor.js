// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.constructor
description: Test for Temporal.Calendar.prototype.constructor.
info: The initial value of Temporal.Calendar.prototype.constructor is %Temporal.Calendar%.
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.Calendar.prototype, "constructor", {
  value: Temporal.Calendar,
  writable: true,
  enumerable: false,
  configurable: true,
});
