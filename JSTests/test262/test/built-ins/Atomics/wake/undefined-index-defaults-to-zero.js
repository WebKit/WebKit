// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  An undefined index arg should translate to 0
info: |
  Atomics.wake( typedArray, index, count )

  2.Let i be ? ValidateAtomicAccess(typedArray, index).
    ...
      2.Let accessIndex be ? ToIndex(requestIndex).

      9.If IsSharedArrayBuffer(buffer) is false, throw a TypeError exception.
        ...
          3.If bufferData is a Data Block, return false

          If value is undefined, then
          Let index be 0.
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

$262.agent.start(
  `
$262.agent.receiveBroadcast(function (sab) { 
  var int32Array = new Int32Array(sab);
  $262.agent.report(Atomics.wait(int32Array, 0, 0, 200));
  $262.agent.leaving();
})


`)

;$262.agent.start(
  `
$262.agent.receiveBroadcast(function (sab) { 
  var int32Array = new Int32Array(sab);
  $262.agent.report(Atomics.wait(int32Array, 0, 0, 200));
  $262.agent.leaving();
})
`);

var sab = new SharedArrayBuffer(4);
var int32Array = new Int32Array(sab);

$262.agent.broadcast(int32Array.buffer);

$262.agent.sleep(100); // halfway through timeout

assert.sameValue(Atomics.wake(int32Array, undefined, 1), 1); // wake at index 0
assert.sameValue(getReport(), "ok");

assert.sameValue(Atomics.wake(int32Array), 1); // wake again at index 0
assert.sameValue(getReport(), "ok");

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null) {
    $262.agent.sleep(100);
  }
  return r;
}
