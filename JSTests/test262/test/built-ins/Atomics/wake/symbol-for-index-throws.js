// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Return abrupt when ToInteger throws for 'index' argument to Atomics.wake
info: |
  Atomics.wake( typedArray, index, value, timeout )

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
  Atomics.wake(int32Array, poisonedValueOf, poisonedValueOf);
});

assert.throws(Test262Error, function() {
  Atomics.wake(int32Array, poisonedToPrimitive, poisonedToPrimitive);
});

assert.throws(TypeError, function() {
  Atomics.wake(int32Array, Symbol("foo"), poisonedValueOf);
});

assert.throws(TypeError, function() {
  Atomics.wake(int32Array, Symbol("foo"), poisonedToPrimitive);
});
