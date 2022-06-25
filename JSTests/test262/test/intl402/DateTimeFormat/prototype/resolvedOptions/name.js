// Copyright (C) 2016 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.resolvedOptions
description: >
  Intl.DateTimeFormat.prototype.resolvedOptions.name is "resolvedOptions".
info: |
  12.4.4 Intl.DateTimeFormat.prototype.resolvedOptions ()

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Intl.DateTimeFormat.prototype.resolvedOptions.name, "resolvedOptions");

verifyNotEnumerable(Intl.DateTimeFormat.prototype.resolvedOptions, "name");
verifyNotWritable(Intl.DateTimeFormat.prototype.resolvedOptions, "name");
verifyConfigurable(Intl.DateTimeFormat.prototype.resolvedOptions, "name");
