// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Throws a TypeError if index arg can not be converted to an Integer
info: |
  Atomics.wait( typedArray, index, value, timeout )

  2. Let i be ? ValidateAtomicAccess(typedArray, index).

  ValidateAtomicAccess( typedArray, requestIndex )

  2. Let accessIndex be ? ToIndex(requestIndex).

  ToIndex ( value )

  2. Else,
    a. Let integerIndex be ? ToInteger(value).

  ToInteger(value)

  1. Let number be ? ToNumber(argument).

    Symbol --> Throw a TypeError exception.

features: [Atomics, SharedArrayBuffer, Symbol, Symbol.toPrimitive, TypedArray]
---*/

var buffer = new SharedArrayBuffer(1024);
var int32Array = new Int32Array(buffer);

var poisonedValueOf = {
  valueOf: function() {
    throw new Test262Error("should not evaluate this code");
  }
};

var poisonedToPrimitive = {
  [Symbol.toPrimitive]: function() {
    throw new Test262Error("passing a poisoned object using @@ToPrimitive");
  }
};

assert.throws(Test262Error, function() {
  Atomics.wait(int32Array, poisonedValueOf, poisonedValueOf, poisonedValueOf);
});

assert.throws(Test262Error, function() {
  Atomics.wait(int32Array, poisonedToPrimitive, poisonedToPrimitive, poisonedToPrimitive);
});

assert.throws(TypeError, function() {
  Atomics.wait(int32Array, Symbol("foo"), poisonedValueOf, poisonedValueOf);
});

assert.throws(TypeError, function() {
  Atomics.wait(int32Array, Symbol("foo"), poisonedToPrimitive, poisonedToPrimitive);
});
