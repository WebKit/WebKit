// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype-@@tostringtag
description: The @@toStringTag property of Temporal.Instant
includes: [propertyHelper.js]
features: [Temporal]
---*/

const { Instant } = Temporal;
verifyProperty(Instant.prototype, Symbol.toStringTag, {
  value: "Temporal.Instant",
  writable: false,
  enumerable: false,
  configurable: true,
});
