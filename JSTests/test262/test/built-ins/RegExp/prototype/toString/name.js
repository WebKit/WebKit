// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 21.2.5.14
description: >
  RegExp.prototype.toString.name is "toString".
info: |
  RegExp.prototype.toString ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(RegExp.prototype.toString.name, "toString");

verifyNotEnumerable(RegExp.prototype.toString, "name");
verifyNotWritable(RegExp.prototype.toString, "name");
verifyConfigurable(RegExp.prototype.toString, "name");
