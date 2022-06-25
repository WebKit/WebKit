// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.setutcfullyear
description: >
  Date.prototype.setUTCFullYear.name is "setUTCFullYear".
info: |
  Date.prototype.setUTCFullYear ( year [ , month [ , date ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.setUTCFullYear.name, "setUTCFullYear");

verifyNotEnumerable(Date.prototype.setUTCFullYear, "name");
verifyNotWritable(Date.prototype.setUTCFullYear, "name");
verifyConfigurable(Date.prototype.setUTCFullYear, "name");
