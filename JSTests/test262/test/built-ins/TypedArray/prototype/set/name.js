// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.set
description: >
  %TypedArray%.prototype.set.name is "set".
info: |
  %TypedArray%.prototype.set ( overloaded [ , offset ])

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

assert.sameValue(TypedArray.prototype.set.name, "set");

verifyNotEnumerable(TypedArray.prototype.set, "name");
verifyNotWritable(TypedArray.prototype.set, "name");
verifyConfigurable(TypedArray.prototype.set, "name");
