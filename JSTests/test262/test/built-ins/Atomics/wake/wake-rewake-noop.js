// Copyright (C) 2018 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test that Atomics.wake on awoken waiter is a noop.
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null) {
    $262.agent.sleep(10);
  }
  return r;
}

function waitUntil(ia, k, value) {
  var i = 0;
  while (Atomics.load(ia, k) !== value && i < 15) {
    $262.agent.sleep(10);
    i++;
  }
  assert.sameValue(Atomics.load(ia, k), value, "All agents are running");
}

$262.agent.start(
`
$262.agent.receiveBroadcast(function(sab) {
  var ia = new Int32Array(sab);
  Atomics.add(ia, 1, 1);
  $262.agent.report(Atomics.wait(ia, 0, 0, 2000));
  $262.agent.leaving();
})
`);

var ia = new Int32Array(new SharedArrayBuffer(2 * Int32Array.BYTES_PER_ELEMENT));
$262.agent.broadcast(ia.buffer);

waitUntil(ia, 1, 1);

assert.sameValue(Atomics.wake(ia, 0, 1), 1);

$262.agent.sleep(10);

// Collect and check results
var report = getReport();

assert.sameValue(report, "ok");

// Already awake, this should be a noop
assert.sameValue(Atomics.wake(ia, 0, 1), 0);

