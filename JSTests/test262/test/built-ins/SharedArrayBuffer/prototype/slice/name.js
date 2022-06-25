// Copyright (C) 2015 André Bargull. All rights reserved.
// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  SharedArrayBuffer.prototype.slice.name is "slice".
info: |
  SharedArrayBuffer.prototype.slice ( start, end )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [SharedArrayBuffer]
---*/

assert.sameValue(SharedArrayBuffer.prototype.slice.name, "slice");

verifyNotEnumerable(SharedArrayBuffer.prototype.slice, "name");
verifyNotWritable(SharedArrayBuffer.prototype.slice, "name");
verifyConfigurable(SharedArrayBuffer.prototype.slice, "name");
