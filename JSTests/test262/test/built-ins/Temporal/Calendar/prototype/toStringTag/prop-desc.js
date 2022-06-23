// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype-@@tostringtag
description: The @@toStringTag property of Temporal.Calendar
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.Calendar.prototype, Symbol.toStringTag, {
  value: "Temporal.Calendar",
  writable: false,
  enumerable: false,
  configurable: true,
});
