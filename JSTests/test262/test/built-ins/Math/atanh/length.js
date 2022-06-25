// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: length property of Math.atanh
es6id: 20.2.2.7
info: |
  Math.atanh ( x )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, has a length
    property whose value is an integer. Unless otherwise specified, this
    value is equal to the largest number of named arguments shown in the
    subclause headings for the function description, including optional
    parameters. However, rest parameters shown using the form “...name”
    are not included in the default argument count.

    Unless otherwise specified, the length property of a built-in Function
    object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
    [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Math.atanh.length, 1);

verifyNotEnumerable(Math.atanh, "length");
verifyNotWritable(Math.atanh, "length");
verifyConfigurable(Math.atanh, "length");
