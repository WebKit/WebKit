// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 21.1.5.2.1
description: >
  %StringIteratorPrototype%.next.length is 0.
info: |
  %StringIteratorPrototype%.next ( )

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
features: [Symbol.iterator]
---*/

var StringIteratorProto = Object.getPrototypeOf(new String()[Symbol.iterator]());

assert.sameValue(StringIteratorProto.next.length, 0);

verifyNotEnumerable(StringIteratorProto.next, "length");
verifyNotWritable(StringIteratorProto.next, "length");
verifyConfigurable(StringIteratorProto.next, "length");
