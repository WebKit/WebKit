// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.filter
description: >
  Array.prototype.filter.name is "filter".
info: |
  Array.prototype.filter ( callbackfn [ , thisArg ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Array.prototype.filter.name, "filter");

verifyNotEnumerable(Array.prototype.filter, "name");
verifyNotWritable(Array.prototype.filter, "name");
verifyConfigurable(Array.prototype.filter, "name");
