// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 21.1.3.1
description: >
  String.prototype.charAt.name is "charAt".
info: |
  String.prototype.charAt ( pos )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(String.prototype.charAt.name, "charAt");

verifyNotEnumerable(String.prototype.charAt, "name");
verifyNotWritable(String.prototype.charAt, "name");
verifyConfigurable(String.prototype.charAt, "name");
