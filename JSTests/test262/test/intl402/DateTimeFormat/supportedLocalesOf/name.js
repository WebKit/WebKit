// Copyright (C) 2016 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.supportedLocalesOf
description: >
  Intl.DateTimeFormat.supportedLocalesOf.name is "supportedLocalesOf".
info: |
  12.3.2 Intl.DateTimeFormat.supportedLocalesOf (locales [ , options ])

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Intl.DateTimeFormat.supportedLocalesOf.name, "supportedLocalesOf");

verifyNotEnumerable(Intl.DateTimeFormat.supportedLocalesOf, "name");
verifyNotWritable(Intl.DateTimeFormat.supportedLocalesOf, "name");
verifyConfigurable(Intl.DateTimeFormat.supportedLocalesOf, "name");
