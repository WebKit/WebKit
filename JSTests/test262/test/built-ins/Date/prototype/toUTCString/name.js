// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.toutcstring
description: >
  Date.prototype.toUTCString.name is "toUTCString".
info: |
  Date.prototype.toUTCString ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.toUTCString.name, "toUTCString");

verifyNotEnumerable(Date.prototype.toUTCString, "name");
verifyNotWritable(Date.prototype.toUTCString, "name");
verifyConfigurable(Date.prototype.toUTCString, "name");
