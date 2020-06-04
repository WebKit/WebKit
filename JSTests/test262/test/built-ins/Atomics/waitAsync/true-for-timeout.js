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
features: [Atomics.waitAsync, SharedArrayBuffer, TypedArray, Atomics, computed-property-names, Symbol, Symbol.toPrimitive, arrow-function]
---*/
const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

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
    Atomics.waitAsync(i32a, 0, 0, true).value,
    Atomics.waitAsync(i32a, 0, 0, valueOf).value,
    Atomics.waitAsync(i32a, 0, 0, toPrimitive).value,
  ]).then(outcomes => {
    assert.sameValue(outcomes[0], 'timed-out');
    assert.sameValue(outcomes[1], 'timed-out');
    assert.sameValue(outcomes[2], 'timed-out');
  }, $DONE).then($DONE, $DONE);
