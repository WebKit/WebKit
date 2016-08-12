// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-@@iterator
es6id: 26.3.2
description: Length of List iterator returned by @@iterator method
info: >
    ES6 Section 17:
    Every built-in Function object, including constructors, has a length
    property whose value is an integer. Unless otherwise specified, this value
    is equal to the largest number of named arguments shown in the subclause
    headings for the function description, including optional parameters.

    [...]

    Unless otherwise specified, the length property of a built-in Function
    object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
    [[Configurable]]: true }.
flags: [module]
includes: [propertyHelper.js]
features: [Symbol.iterator]
---*/

import * as ns from './list-iter-next-length.js';

var next = ns[Symbol.iterator]().next;

assert.sameValue(next.length, 0);

verifyNotEnumerable(next, 'length');
verifyNotWritable(next, 'length');
verifyConfigurable(next, 'length');
