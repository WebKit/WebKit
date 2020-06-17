// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.waitasync
description: >
  Undefined timeout arg is coerced to zero
info: |
  Atomics.waitAsync( typedArray, index, value, timeout )

  1. Return DoWait(async, typedArray, index, value, timeout).

  DoWait ( mode, typedArray, index, value, timeout )

  6. Let q be ? ToNumber(timeout).
    ...
    Undefined    Return NaN.

  5.If q is NaN, let t be +∞, else let t be max(q, 0)

flags: [async]
includes: [atomicsHelper.js]
features: [Atomics.waitAsync, SharedArrayBuffer, TypedArray, Atomics, computed-property-names, Symbol, Symbol.toPrimitive, arrow-function]
---*/
assert.sameValue(typeof Atomics.waitAsync, 'function');
const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

$262.agent.start(`
  $262.agent.receiveBroadcast(async (sab) => {
    var i32a = new Int32Array(sab);
    $262.agent.sleep(1000);
    Atomics.notify(i32a, 0, 4);
    $262.agent.leaving();
  });
`);

$262.agent.safeBroadcast(i32a);

const valueOf = {
  valueOf() {
    return undefined;
  }
};

const toPrimitive = {
  [Symbol.toPrimitive]() {
    return undefined;
  }
};

Promise.all([
    Atomics.waitAsync(i32a, 0, 0).value,
    Atomics.waitAsync(i32a, 0, 0, undefined).value,
    Atomics.waitAsync(i32a, 0, 0, valueOf).value,
    Atomics.waitAsync(i32a, 0, 0, toPrimitive).value,
  ]).then(outcomes => {
    assert.sameValue(outcomes[0], 'ok');
    assert.sameValue(outcomes[1], 'ok');
    assert.sameValue(outcomes[2], 'ok');
    assert.sameValue(outcomes[3], 'ok');
  }, $DONE).then($DONE, $DONE);
