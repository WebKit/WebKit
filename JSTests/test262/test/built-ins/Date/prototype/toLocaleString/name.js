// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.tolocalestring
description: >
  Date.prototype.toLocaleString.name is "toLocaleString".
info: |
  Date.prototype.toLocaleString ( [ reserved1 [ , reserved2 ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.toLocaleString.name, "toLocaleString");

verifyNotEnumerable(Date.prototype.toLocaleString, "name");
verifyNotWritable(Date.prototype.toLocaleString, "name");
verifyConfigurable(Date.prototype.toLocaleString, "name");
