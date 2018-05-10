// Copyright (C) 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Get the correct WaiterList
info: |
  Atomics.wait( typedArray, index, value, timeout )

  ...
  11. Let WL be GetWaiterList(block, indexedPosition).
  ...


  GetWaiterList( block, i )

  ...
  4. Return the WaiterList that is referenced by the pair (block, i).

features: [Atomics, SharedArrayBuffer, TypedArray]
---*/
function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null) {
    $262.agent.sleep(10);
  }
  return r;
}

$262.agent.start(`
$262.agent.receiveBroadcast(function(sab) {
  var i32 = new Int32Array(sab);

  // Wait on index 0
  Atomics.wait(i32, 0, 0, 200);
  $262.agent.report("fail");
  $262.agent.leaving();
});
`);

$262.agent.start(`
$262.agent.receiveBroadcast(function(sab) {
  var i32 = new Int32Array(sab);

  // Wait on index 2
  Atomics.wait(i32, 2, 0, 200);
  $262.agent.report("pass");
  $262.agent.leaving();
});
`);

var length = 4 * Int32Array.BYTES_PER_ELEMENT;
var i32 = new Int32Array(new SharedArrayBuffer(length));

$262.agent.broadcast(i32.buffer);
$262.agent.sleep(10);

// Wake index 2
Atomics.wake(i32, 2, 1);

assert.sameValue(getReport(), "pass");
