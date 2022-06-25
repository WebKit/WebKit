// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.2.5.6
description: RegExp.prototype[Symbol.match] `length` property
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
includes: [propertyHelper.js]
features: [Symbol.match]
---*/

assert.sameValue(RegExp.prototype[Symbol.match].length, 1);

verifyNotEnumerable(RegExp.prototype[Symbol.match], 'length');
verifyNotWritable(RegExp.prototype[Symbol.match], 'length');
verifyConfigurable(RegExp.prototype[Symbol.match], 'length');
