// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.1.3.6
description: >
  Number.prototype.toString.name is "toString".
info: |
  Number.prototype.toString ( [ radix ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Number.prototype.toString.name, "toString");

verifyNotEnumerable(Number.prototype.toString, "name");
verifyNotWritable(Number.prototype.toString, "name");
verifyConfigurable(Number.prototype.toString, "name");
