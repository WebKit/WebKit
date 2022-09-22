// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group errors when array-like's length can't be coerced.
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  2. Let len be ? LengthOfArrayLike(O).

  ...
features: [array-grouping]
---*/

assert.throws(Test262Error, function() {
  const arrayLike = Object.defineProperty({}, 'length', {
    get: function() {
      throw new Test262Error('no length for you');
    }
  });
  Array.prototype.group.call(arrayLike, function() {
    return 'key';
  });
});

assert.throws(TypeError, function() {
  const arrayLike = {
    length: 1n,
  };
  Array.prototype.group.call(arrayLike, function() {
    return 'key';
  });
});
