// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.waitasync
description: >
  Atomics.waitAsync returns a result object containing a promise that resolves to "timed-out" and async is true.
info: |
  Atomics.waitAsync( typedArray, index, value, timeout )

  1. Return DoWait(async, typedArray, index, value, timeout).

  DoWait ( mode, typedArray, index, value, timeout )

  ...
  13. Let promiseCapability be undefined.
  14. If mode is async, then
    a. Set promiseCapability to ! NewPromiseCapability(%Promise%).

  ...
  Perform ! CreateDataPropertyOrThrow(_resultObject_, *"async"*, *true*).
  Perform ! CreateDataPropertyOrThrow(_resultObject_, *"value"*, _promiseCapability_.[[Promise]]).
  Return _resultObject_.

flags: [async]
features: [Atomics.waitAsync, TypedArray, SharedArrayBuffer, destructuring-binding, Atomics, arrow-function]
---*/

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 8)
);

let {async, value} = Atomics.waitAsync(i32a, 0, 0, 1);
assert.sameValue(async, true);
assert(value instanceof Promise);
assert.sameValue(Object.getPrototypeOf(value), Promise.prototype);

value.then(outcome => {
  assert.sameValue(outcome, "timed-out");
}).then(() => $DONE(), $DONE);



