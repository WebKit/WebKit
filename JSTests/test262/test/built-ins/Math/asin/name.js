// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.2.2.4
description: >
  Math.asin.name is "asin".
info: |
  Math.asin ( x )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Math.asin.name, "asin");

verifyNotEnumerable(Math.asin, "name");
verifyNotWritable(Math.asin, "name");
verifyConfigurable(Math.asin, "name");
