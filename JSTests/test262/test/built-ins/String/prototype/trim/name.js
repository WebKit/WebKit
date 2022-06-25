// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 21.1.3.25
description: >
  String.prototype.trim.name is "trim".
info: |
  String.prototype.trim ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(String.prototype.trim.name, "trim");

verifyNotEnumerable(String.prototype.trim, "name");
verifyNotWritable(String.prototype.trim, "name");
verifyConfigurable(String.prototype.trim, "name");
