// Copyright (C) 2021 Microsoft. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.findLast
description: >
  "findLast" property of TypedArrayPrototype
info: |
  ES6 section 17: Every other data property described in clauses 18 through 26
  and in Annex B.2 has the attributes { [[Writable]]: true,
  [[Enumerable]]: false, [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js, testTypedArray.js]
features: [TypedArray, array-find-from-last]
---*/

var TypedArrayPrototype = TypedArray.prototype;

verifyProperty(TypedArray.prototype, "findLast", {
  enumerable: false,
  writable: true,
  configurable: true
});
