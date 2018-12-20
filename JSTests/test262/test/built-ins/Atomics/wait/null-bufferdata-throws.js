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

const i32a = new Int32Array(
  new ArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

const poisoned = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

try {
  $DETACHBUFFER(i32a.buffer); // Detaching a non-shared ArrayBuffer sets the [[ArrayBufferData]] value to null
} catch (error) {
  $ERROR(`An unexpected error occurred when detaching ArrayBuffer: ${error.message}`);
}

assert.throws(TypeError, function() {
  Atomics.wait(i32a, poisoned, poisoned, poisoned);
}, '`Atomics.wait(i32a, poisoned, poisoned, poisoned)` throws TypeError');
