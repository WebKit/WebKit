// Copyright (C) 2016 The V8 Project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-isnan-number
description: >
  The length property of isNaN is 1
includes: [propertyHelper.js]
---*/

verifyProperty(isNaN, "length", {
  value: 1,
  writable: false,
  enumerable: false,
  configurable: true
});
