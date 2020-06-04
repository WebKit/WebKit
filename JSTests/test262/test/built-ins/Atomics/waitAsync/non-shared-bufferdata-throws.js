// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.waitasync
description: >
  Throws a TypeError if typedArray.buffer is not a SharedArrayBuffer
info: |
  Atomics.waitAsync( typedArray, index, value, timeout )

  1. Return DoWait(async, typedArray, index, value, timeout).

  DoWait ( mode, typedArray, index, value, timeout )

  1. Let buffer be ? ValidateSharedIntegerTypedArray(typedArray, true).

  ValidateSharedIntegerTypedArray ( typedArray [ , waitable ] )

  5. If waitable is true, then
    a. If typeName is not "Int32Array" or "BigInt64Array", throw a TypeError exception.

features: [Atomics.waitAsync, ArrayBuffer, Atomics, TypedArray, arrow-function]
---*/
const i32a = new Int32Array(
  new ArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

const poisoned = {
  valueOf() {
    throw new Test262Error('should not evaluate this code');
  }
};

assert.throws(TypeError, () => {
  Atomics.wait(i32a, 0, 0, 0);
}, '`Atomics.wait(i32a, 0, 0, 0)` throws TypeError');

assert.throws(TypeError, () => {
  Atomics.wait(i32a, poisoned, poisoned, poisoned);
}, '`Atomics.wait(i32a, poisoned, poisoned, poisoned)` throws TypeError');
