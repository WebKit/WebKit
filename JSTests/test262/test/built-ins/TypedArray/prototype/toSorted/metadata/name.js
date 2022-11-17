// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.toSorted
description: >
  %TypedArray%.prototype.toSorted.name is "toSorted".
info: |
  %TypedArray%.prototype.toSorted ( comparefn )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
features: [TypedArray, change-array-by-copy]
---*/

verifyProperty(TypedArray.prototype.toSorted, "name", {
  value: "toSorted",
  writable: false,
  enumerable: false,
  configurable: true,
});
