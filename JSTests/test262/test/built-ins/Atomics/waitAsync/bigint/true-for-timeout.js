// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.waitasync
description: >
  Throws a TypeError if index arg can not be converted to an Integer
info: |
  Atomics.waitAsync( typedArray, index, value, timeout )

  1. Return DoWait(async, typedArray, index, value, timeout).

  DoWait ( mode, typedArray, index, value, timeout )

  6. Let q be ? ToNumber(timeout).

    Boolean -> If argument is true, return 1. If argument is false, return +0.

flags: [async]
features: [Atomics.waitAsync, SharedArrayBuffer, TypedArray, Atomics, BigInt, computed-property-names, Symbol, Symbol.toPrimitive, arrow-function]
---*/
assert.sameValue(typeof Atomics.waitAsync, 'function');
const i64a = new BigInt64Array(new SharedArrayBuffer(BigInt64Array.BYTES_PER_ELEMENT * 4));

const valueOf = {
  valueOf() {
    return true;
  }
};

const toPrimitive = {
  [Symbol.toPrimitive]() {
    return true;
  }
};

Promise.all([
  Atomics.waitAsync(i64a, 0, 0n, true).value,
  Atomics.waitAsync(i64a, 0, 0n, valueOf).value,
  Atomics.waitAsync(i64a, 0, 0n, toPrimitive).value
]).then(outcomes => {
  assert.sameValue(outcomes[0], 'timed-out');
  assert.sameValue(outcomes[1], 'timed-out');
  assert.sameValue(outcomes[2], 'timed-out');
}).then($DONE, $DONE);
