// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.2.3.1
description: >
  Function.prototype.apply.name is "apply".
info: |
  Function.prototype.apply ( thisArg, argArray )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Function.prototype.apply.name, "apply");

verifyNotEnumerable(Function.prototype.apply, "name");
verifyNotWritable(Function.prototype.apply, "name");
verifyConfigurable(Function.prototype.apply, "name");
