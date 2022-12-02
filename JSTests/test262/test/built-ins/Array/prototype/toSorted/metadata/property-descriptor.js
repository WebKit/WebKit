// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.toSorted
description: >
  "toSorted" property of Array.prototype
info: |
  17 ECMAScript Standard Built-in Objects

  Every other data property described in clauses 18 through 26 and in Annex B.2
  has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [change-array-by-copy]
---*/

assert.sameValue(typeof Array.prototype.toSorted, "function", "typeof");

verifyProperty(Array.prototype, "toSorted", {
  value: Array.prototype.toSorted,
  writable: true,
  enumerable: false,
  configurable: true,
});
