// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-calendar-prototype
description: The "prototype" property of Temporal.Calendar
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(typeof Temporal.Calendar.prototype, "object");
assert.notSameValue(Temporal.Calendar.prototype, null);

verifyProperty(Temporal.Calendar, "prototype", {
  writable: false,
  enumerable: false,
  configurable: false,
});
