// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-instant-objects
description: The "prototype" property of Temporal.Instant
includes: [propertyHelper.js]
features: [Temporal]
---*/

const { Instant } = Temporal;

assert.sameValue(typeof Instant.prototype, "object");
assert.notSameValue(Instant.prototype, null);

verifyProperty(Instant, "prototype", {
  writable: false,
  enumerable: false,
  configurable: false,
});
