// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetime
description: The "zonedDateTime" property of Temporal.Now
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(typeof Temporal.Now.zonedDateTime, "function", "typeof is function");

verifyProperty(Temporal.Now, 'zonedDateTime', {
  enumerable: false,
  writable: true,
  configurable: true
});
