// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.1.2.12
description: >
  Object.isFrozen.name is "isFrozen".
info: |
  Object.isFrozen ( O )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.isFrozen.name, "isFrozen");

verifyNotEnumerable(Object.isFrozen, "name");
verifyNotWritable(Object.isFrozen, "name");
verifyConfigurable(Object.isFrozen, "name");
