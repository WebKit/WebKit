// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 25.3.1.3
description: >
  Generator.prototype.return.name is "return".
info: |
  Generator.prototype.return ( value )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [generators]
---*/

function* g() {}
var GeneratorPrototype = Object.getPrototypeOf(g).prototype;

assert.sameValue(GeneratorPrototype.return.name, "return");

verifyNotEnumerable(GeneratorPrototype.return, "name");
verifyNotWritable(GeneratorPrototype.return, "name");
verifyConfigurable(GeneratorPrototype.return, "name");
