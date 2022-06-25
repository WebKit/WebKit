// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 21.1.5.2.1
description: >
  %StringIteratorPrototype%.next.name is "next".
info: |
  %StringIteratorPrototype%.next ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.iterator]
---*/

var StringIteratorProto = Object.getPrototypeOf(new String()[Symbol.iterator]());

assert.sameValue(StringIteratorProto.next.name, "next");

verifyNotEnumerable(StringIteratorProto.next, "name");
verifyNotWritable(StringIteratorProto.next, "name");
verifyConfigurable(StringIteratorProto.next, "name");
