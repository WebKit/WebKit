// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.1.2.15
description: >
  Object.preventExtensions.name is "preventExtensions".
info: |
  Object.preventExtensions ( O )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.preventExtensions.name, "preventExtensions");

verifyNotEnumerable(Object.preventExtensions, "name");
verifyNotWritable(Object.preventExtensions, "name");
verifyConfigurable(Object.preventExtensions, "name");
