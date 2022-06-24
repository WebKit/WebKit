// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype-@@tostringtag
description: The @@toStringTag property of Temporal.TimeZone
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.TimeZone.prototype, Symbol.toStringTag, {
  value: "Temporal.TimeZone",
  writable: false,
  enumerable: false,
  configurable: true,
});
