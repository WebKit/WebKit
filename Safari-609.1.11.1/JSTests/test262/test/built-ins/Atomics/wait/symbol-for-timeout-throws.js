// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Throws a TypeError if index arg can not be converted to an Integer
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4. Let q be ? ToNumber(timeout).

    Symbol --> Throw a TypeError exception.

features: [Atomics, SharedArrayBuffer, Symbol, Symbol.toPrimitive, TypedArray]
---*/

var buffer = new SharedArrayBuffer(1024);
var i32a = new Int32Array(buffer);

var poisonedValueOf = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

var poisonedToPrimitive = {
  [Symbol.toPrimitive]: function() {
    throw new Test262Error('passing a poisoned object using @@ToPrimitive');
  }
};

assert.throws(Test262Error, function() {
  Atomics.wait(i32a, 0, 0, poisonedValueOf);
}, '`Atomics.wait(i32a, 0, 0, poisonedValueOf)` throws Test262Error');

assert.throws(Test262Error, function() {
  Atomics.wait(i32a, 0, 0, poisonedToPrimitive);
}, '`Atomics.wait(i32a, 0, 0, poisonedToPrimitive)` throws Test262Error');

assert.throws(TypeError, function() {
  Atomics.wait(i32a, 0, 0, Symbol("foo"));
}, '`Atomics.wait(i32a, 0, 0, Symbol("foo"))` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wait(i32a, 0, 0, Symbol("foo"));
}, '`Atomics.wait(i32a, 0, 0, Symbol("foo"))` throws TypeError');
