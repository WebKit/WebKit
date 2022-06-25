// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 23.1.5.2.1
description: >
  %MapIteratorPrototype%.next.name is "next".
info: |
  %MapIteratorPrototype%.next ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

var MapIteratorProto = Object.getPrototypeOf(new Map().values());

assert.sameValue(MapIteratorProto.next.name, "next");

verifyNotEnumerable(MapIteratorProto.next, "name");
verifyNotWritable(MapIteratorProto.next, "name");
verifyConfigurable(MapIteratorProto.next, "name");
