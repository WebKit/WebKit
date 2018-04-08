// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.wait
description: >
  Test that Atomics.wait returns the right result when it was awoken before
  a timeout
info: |
  Atomics.wait( typedArray, index, value, timeout )

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

var timeout = 20000;
$262.agent.start(
    `
  $262.agent.receiveBroadcast(function (sab) { 
    var int32Array = new Int32Array(sab);
    $262.agent.report(Atomics.wait(int32Array, 0, 0, ${timeout}));
    $262.agent.leaving();
  })
  `);
  
  var sab = new SharedArrayBuffer(4);
  var int32Array = new Int32Array(sab);
  
  var sleeping = 100;

  $262.agent.broadcast(int32Array.buffer);
  $262.agent.sleep(sleeping);
  
  assert.sameValue(Atomics.wake(int32Array, 0), 1);
  
  assert.sameValue(getReport(), "ok");
  assert(sleeping < timeout, "this test assumes it won't last for more than 20 seconds");
  
  function getReport() {
    var r;
    while ((r = $262.agent.getReport()) == null) {
      sleeping += 100;
      $262.agent.sleep(100);
    }
    return r;
  }
