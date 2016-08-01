// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-@@iterator
es6id: 26.3.2
description: Length of @@iterator method
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

import * as ns from './length.js';

assert.sameValue(ns[Symbol.iterator].length, 0);

verifyNotEnumerable(ns[Symbol.iterator], 'length');
verifyNotWritable(ns[Symbol.iterator], 'length');
verifyConfigurable(ns[Symbol.iterator], 'length');
