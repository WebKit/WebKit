// Copyright (C) 2015 André Bargull. All rights reserved.
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  SharedArrayBuffer.prototype.slice.length is 2.
info: |
  SharedArrayBuffer.prototype.slice ( start, end )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, has a length
    property whose value is an integer. Unless otherwise specified, this
    value is equal to the largest number of named arguments shown in the
    subclause headings for the function description, including optional
    parameters. However, rest parameters shown using the form “...name”
    are not included in the default argument count.

    Unless otherwise specified, the length property of a built-in Function
    object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
    [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [SharedArrayBuffer]
---*/

assert.sameValue(SharedArrayBuffer.prototype.slice.length, 2);

verifyNotEnumerable(SharedArrayBuffer.prototype.slice, "length");
verifyNotWritable(SharedArrayBuffer.prototype.slice, "length");
verifyConfigurable(SharedArrayBuffer.prototype.slice, "length");
