// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.1.3.7
description: >
  Object.prototype.valueOf.name is "valueOf".
info: |
  Object.prototype.valueOf ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.prototype.valueOf.name, "valueOf");

verifyNotEnumerable(Object.prototype.valueOf, "name");
verifyNotWritable(Object.prototype.valueOf, "name");
verifyConfigurable(Object.prototype.valueOf, "name");
