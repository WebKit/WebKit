// Copyright (C) 2017 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.shift
description: >
  "shift" property of Array.prototype
info: |
  17 ECMAScript Standard Built-in Objects
  
  Every other data property described in clauses 18 through 26 and in Annex B.2
  has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
---*/

assert.sameValue(typeof Array.prototype.shift, 'function', 'typeof');

verifyNotEnumerable(Array.prototype, "shift");
verifyWritable(Array.prototype, "shift");
verifyConfigurable(Array.prototype, "shift");
