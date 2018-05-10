// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Returns "not-equal" when value arg does not match an index in the typedArray
info: |
  Atomics.wait( typedArray, index, value, timeout )

  3.Let v be ? ToInt32(value).
    ...
  14.If v is not equal to w, then
    a.Perform LeaveCriticalSection(WL).
    b. Return the String "not-equal".

features: [Atomics, SharedArrayBuffer, TypedArray]
includes: [atomicsHelper.js]
---*/

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null) {
    $262.agent.sleep(100);
  }
  return r;
}

var value = 42;

$262.agent.start(
`
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);

  $262.agent.report(Atomics.store(int32Array, 0, ${value}));

  $262.agent.report(Atomics.wait(int32Array, 0, 0));

  $262.agent.leaving();
})
`);

var int32Array = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));

$262.agent.broadcast(int32Array.buffer);

assert.sameValue(getReport(), value.toString());
assert.sameValue(getReport(), "not-equal");

