// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.toSorted
description: >
  "toSorted" property of %TypedArray%.prototype
info: |
  17 ECMAScript Standard Built-in Objects

  Every other data property described in clauses 18 through 26 and in Annex B.2
  has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js, testTypedArray.js]
features: [TypedArray, change-array-by-copy]
---*/

assert.sameValue(typeof TypedArray.prototype.toSorted, "function", "typeof");

verifyProperty(TypedArray.prototype, "toSorted", {
  value: TypedArray.prototype.toSorted,
  writable: true,
  enumerable: false,
  configurable: true,
});
