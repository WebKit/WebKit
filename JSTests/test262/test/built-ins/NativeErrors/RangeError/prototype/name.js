// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.5.6.3.3
description: >
  The initial value of RangeError.prototype.name is "RangeError".
info: |
  The initial value of the name property of the prototype for a given NativeError
  constructor is a string consisting of the name of the constructor (the name used
  instead of NativeError).

  17 ECMAScript Standard Built-in Objects:
    Every other data property described in clauses 18 through 26 and in Annex B.2 has
    the attributes { [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true }
    unless otherwise specified.
includes: [propertyHelper.js]
---*/

assert.sameValue(RangeError.prototype.name, "RangeError");

verifyNotEnumerable(RangeError.prototype, "name");
verifyWritable(RangeError.prototype, "name");
verifyConfigurable(RangeError.prototype, "name");
