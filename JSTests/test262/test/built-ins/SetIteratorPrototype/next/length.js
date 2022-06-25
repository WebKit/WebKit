// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 23.2.5.2.1
description: >
  %SetIteratorPrototype%.next.length is 0.
info: |
  %SetIteratorPrototype%.next ( )

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

var SetIteratorProto = Object.getPrototypeOf(new Set().values());

assert.sameValue(SetIteratorProto.next.length, 0);

verifyNotEnumerable(SetIteratorProto.next, "length");
verifyNotWritable(SetIteratorProto.next, "length");
verifyConfigurable(SetIteratorProto.next, "length");
