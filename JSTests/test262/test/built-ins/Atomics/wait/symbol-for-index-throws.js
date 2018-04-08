// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Throws a TypeError if index arg can not be converted to an Integer
info: |
  Atomics.wait( typedArray, index, value, timeout )

  3.Let v be ? ToInt32(value).
    ...
      1.Let number be ? ToNumber(argument).
        Symbol --> Throw a TypeError exception.
features: [ Atomics, SharedArrayBuffer, TypedArray, Symbol, Symbol.toPrimitive]
---*/

var sab = new SharedArrayBuffer(1024);
var int32Array = new Int32Array(sab);

var poisoned = {
  valueOf: function() {
    throw new Test262Error("should not evaluate this code");
  }
};

var poisonedWithString = {
  get valueOf() { throw "should not evaluate this code"; }
};

var poisonedToPrimitive = {
  get [Symbol.ToPrimitive]() {
    throw new Test262Error('passing a poisoned object using @@ToPrimitive');
  }
};

assert.throws(TypeError, function() {
  Atomics.wait(int32Array, Symbol('foo'), poisonedWithString, poisonedWithString)
}, 'Symbol');

assert.throws(Test262Error, function() {
  Atomics.wait(int32Array, poisoned, poisonedWithString, poisonedWithString)
}, 'passing a poisoned object using valueOf');

assert.throws(Test262Error, function() {
  Atomics.wait(int32Array, poisoned, poisonedToPrimitive, poisonedToPrimitive);
}, 'passing a poisoned object using @@ToPrimitive');
