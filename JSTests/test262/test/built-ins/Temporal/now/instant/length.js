// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.instant
description: Temporal.now.instant.length is 0
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.now.instant, "length", {
  value: 0,
  writable: false,
  enumerable: false,
  configurable: true,
});
