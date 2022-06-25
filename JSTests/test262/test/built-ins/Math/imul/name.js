// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.2.2.19
description: >
  Math.imul.name is "imul".
info: |
  Math.imul ( x, y )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Math.imul.name, "imul");

verifyNotEnumerable(Math.imul, "name");
verifyNotWritable(Math.imul, "name");
verifyConfigurable(Math.imul, "name");
