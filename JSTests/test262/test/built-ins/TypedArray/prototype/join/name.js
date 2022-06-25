// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.join
description: >
  %TypedArray%.prototype.join.name is "join".
info: |
  %TypedArray%.prototype.join ( separator )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
features: [TypedArray]
---*/

assert.sameValue(TypedArray.prototype.join.name, "join");

verifyNotEnumerable(TypedArray.prototype.join, "name");
verifyNotWritable(TypedArray.prototype.join, "name");
verifyConfigurable(TypedArray.prototype.join, "name");
