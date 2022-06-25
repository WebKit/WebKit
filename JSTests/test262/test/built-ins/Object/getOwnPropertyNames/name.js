// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.1.2.7
description: >
  Object.getOwnPropertyNames.name is "getOwnPropertyNames".
info: |
  Object.getOwnPropertyNames ( O )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.getOwnPropertyNames.name, "getOwnPropertyNames");

verifyNotEnumerable(Object.getOwnPropertyNames, "name");
verifyNotWritable(Object.getOwnPropertyNames, "name");
verifyConfigurable(Object.getOwnPropertyNames, "name");
