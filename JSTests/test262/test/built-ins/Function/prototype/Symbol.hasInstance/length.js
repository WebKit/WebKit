// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.2.3.6
description: Function.prototye[Symbol.hasInstance] `length` property
info: |
    ES6 Section 17:
    Every built-in Function object, including constructors, has a length
    property whose value is an integer. Unless otherwise specified, this value
    is equal to the largest number of named arguments shown in the subclause
    headings for the function description, including optional parameters.

    [...]

    Unless otherwise specified, the length property of a built-in Function
    object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
    [[Configurable]]: true }.
features: [Symbol.hasInstance]
includes: [propertyHelper.js]
---*/

assert.sameValue(Function.prototype[Symbol.hasInstance].length, 1);

verifyNotEnumerable(Function.prototype[Symbol.hasInstance], 'length');
verifyNotWritable(Function.prototype[Symbol.hasInstance], 'length');
verifyConfigurable(Function.prototype[Symbol.hasInstance], 'length');
