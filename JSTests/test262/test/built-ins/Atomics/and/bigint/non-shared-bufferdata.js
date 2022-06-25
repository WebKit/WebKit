// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.and
description: >
  Atomics.and will operate on TA when TA.buffer is not a SharedArrayBuffer
includes: [testBigIntTypedArray.js]
features: [ArrayBuffer, Atomics, BigInt, TypedArray]
---*/
testWithBigIntTypedArrayConstructors(TA => {
  const buffer = new ArrayBuffer(TA.BYTES_PER_ELEMENT * 4);
  const view = new TA(buffer);
  assert.sameValue(Atomics.and(view, 0, 1n), 0n, 'Atomics.and(view, 0, 1n) returns 0n');
  assert.sameValue(Atomics.load(view, 0), 0n, 'Atomics.load(view, 0) returns 0n');
});
