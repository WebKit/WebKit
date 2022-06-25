// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-date.prototype-@@toprimitive
description: Date.prototype[Symbol.toPrimitive] `length` property
info: |
    ES6 section 17:

    Every built-in Function object, including constructors, has a length
    property whose value is an integer. Unless otherwise specified, this value
    is equal to the largest number of named arguments shown in the subclause
    headings for the function description, including optional parameters.

    [...]

    Unless otherwise specified, the length property of a built-in Function
    object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
    [[Configurable]]: true }.
features: [Symbol.toPrimitive]
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype[Symbol.toPrimitive].length, 1);

verifyNotEnumerable(Date.prototype[Symbol.toPrimitive], 'length');
verifyNotWritable(Date.prototype[Symbol.toPrimitive], 'length');
verifyConfigurable(Date.prototype[Symbol.toPrimitive], 'length');
