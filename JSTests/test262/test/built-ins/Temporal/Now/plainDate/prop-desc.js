// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindate
description: The "plainDate" property of Temporal.Now
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(typeof Temporal.Now.plainDate, "function", "typeof is function");

verifyProperty(Temporal.Now, "plainDate", {
  enumerable: false,
  writable: true,
  configurable: true
});
