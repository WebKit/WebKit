// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.concat
description: >
  TypeError is thrown if CreateDataProperty fails.
  (result object's "0" is non-configurable, argument is non-spreadable)
info: |
  Array.prototype.concat ( ...arguments )

  [...]
  5. Repeat, while items is not empty
    [...]
    c. If spreadable is true, then
      [...]
    d. Else,
      [...]
      iii. Perform ? CreateDataPropertyOrThrow(A, ! ToString(n), E).
    [...]

  CreateDataPropertyOrThrow ( O, P, V )

  [...]
  3. Let success be ? CreateDataProperty(O, P, V).
  4. If success is false, throw a TypeError exception.
features: [Symbol.species]
---*/

var A = function(_length) {
  Object.defineProperty(this, "0", {
    set: function(_value) {},
    configurable: false,
  });
};

var arr = [];
arr.constructor = {};
arr.constructor[Symbol.species] = A;

assert.throws(TypeError, function() {
  arr.concat(1);
}, 'arr.concat(1) throws a TypeError exception');
