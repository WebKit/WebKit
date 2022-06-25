// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getdate
description: >
  Date.prototype.getDate.name is "getDate".
info: |
  Date.prototype.getDate ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.getDate.name, "getDate");

verifyNotEnumerable(Date.prototype.getDate, "name");
verifyNotWritable(Date.prototype.getDate, "name");
verifyConfigurable(Date.prototype.getDate, "name");
