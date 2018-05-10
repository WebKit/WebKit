// Copyright (C) 2018 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test that Atomics.wake wakes zero waiters if there are no agents that match
  its arguments waiting.
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/


$262.agent.start(
`
$262.agent.receiveBroadcast(function (sab) {
  var ia = new Int32Array(sab);
  Atomics.add(ia, 1, 1);
  $262.agent.leaving();
})
`);

var ia = new Int32Array(new SharedArrayBuffer(2 * Int32Array.BYTES_PER_ELEMENT));
$262.agent.broadcast(ia.buffer);

waitUntil(ia, 1);

// There are ZERO matching agents...
assert.sameValue(Atomics.wake(ia, 1, 1), 0);

function waitUntil(ia, k) {
  var i = 0;
  while (Atomics.load(ia, k) !== 1 && i < 15) {
    $262.agent.sleep(100);
    i++;
  }
  assert.sameValue(Atomics.load(ia, k), 1, "All agents are running");
}
