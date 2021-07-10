// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.instant
description: Temporal.now.instant.name is "instant".
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(Temporal.now.instant.name, 'instant');

verifyProperty(Temporal.now.instant, 'name', {
  enumerable: false,
  writable: false,
  configurable: true
});
