// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.flat
description: >
  TypeError is thrown if CreateDataProperty fails.
  (result object is non-extensible, source array is not flattened)
info: |
  Array.prototype.flat ( [ depth ] )

  [...]
  6. Perform ? FlattenIntoArray(A, O, sourceLen, 0, depthNum).

  FlattenIntoArray ( target, source, sourceLen, start, depth [ , mapperFunction, thisArg ] )

  [...]
  9. Repeat, while sourceIndex < sourceLen
    [...]
    c. If exists is true, then
      [...]
      v. If shouldFlatten is true, then
        [...]
      vi. Else,
        [...]
        2. Perform ? CreateDataPropertyOrThrow(target, ! ToString(targetIndex), element).
      [...]

  CreateDataPropertyOrThrow ( O, P, V )

  [...]
  3. Let success be ? CreateDataProperty(O, P, V).
  4. If success is false, throw a TypeError exception.
features: [Symbol.species]
---*/

var A = function(_length) {
  this.length = 0;
  Object.preventExtensions(this);
};

var arr = [1];
arr.constructor = {};
arr.constructor[Symbol.species] = A;

assert.throws(TypeError, function() {
  arr.flat(1);
});
