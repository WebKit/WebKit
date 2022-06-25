// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-prototype
description: The "prototype" property of Temporal.TimeZone
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(typeof Temporal.TimeZone.prototype, "object");
assert.notSameValue(Temporal.TimeZone.prototype, null);

verifyProperty(Temporal.TimeZone, "prototype", {
  writable: false,
  enumerable: false,
  configurable: false,
});
