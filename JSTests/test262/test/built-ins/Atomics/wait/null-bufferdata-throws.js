// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.wait
description: >
  A null value for bufferData throws a TypeError
info: |
  Atomics.wait( typedArray, index, value, timeout )

  1.Let buffer be ? ValidateSharedIntegerTypedArray(typedArray, true).
    ...
      9.If IsSharedArrayBuffer(buffer) is false, throw a TypeError exception.
        ...
          3.If bufferData is null, return false.
includes: [detachArrayBuffer.js]
features: [ArrayBuffer, Atomics, TypedArray]
---*/

var int32Array = new Int32Array(new ArrayBuffer(1024));
var poisoned = {
  valueOf: function() {
    throw new Test262Error("should not evaluate this code");
  }
};

$DETACHBUFFER(int32Array.buffer); // Detaching a non-shared ArrayBuffer sets the [[ArrayBufferData]] value to null

assert.throws(TypeError, function() {
  Atomics.wait(int32Array, poisoned, poisoned, poisoned);
});
