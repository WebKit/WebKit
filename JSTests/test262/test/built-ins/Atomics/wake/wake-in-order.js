// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test that Atomics.wake wakes agents in the order they are waiting.
features: [Atomics]
---*/

var NUMAGENT = 3;

var WAKEUP = 0;                 // Waiters on this will be woken
var SPIN = 1;                   // Worker i (zero-based) spins on location SPIN+i
var RUNNING = SPIN + NUMAGENT;  // Accounting of live agents
var NUMELEM = RUNNING + 1;

// Create workers and start them all spinning.  We set atomic slots to make
// them go into a wait, thus controlling the waiting order.  Then we wake them
// one by one and observe the wakeup order.

for ( var i=0 ; i < NUMAGENT ; i++ ) {
$262.agent.start(
`
$262.agent.receiveBroadcast(function (sab) {
  var ia = new Int32Array(sab);
  Atomics.add(ia, ${RUNNING}, 1);
  while (Atomics.load(ia, ${SPIN + i}) === 0)
      /* nothing */ ;
  $262.agent.report(${i} + Atomics.wait(ia, ${WAKEUP}, 0));
  $262.agent.leaving();
})
`);
}

var ia = new Int32Array(new SharedArrayBuffer(NUMELEM * Int32Array.BYTES_PER_ELEMENT));
$262.agent.broadcast(ia.buffer);

// Wait for agents to be running.
waitUntil(ia, RUNNING, NUMAGENT);

// Then wait some more to give the agents a fair chance to wait.  If we don't,
// we risk sending the wakeup before agents are sleeping, and we hang.
$262.agent.sleep(500);

// Make them sleep in order 0 1 2 on ia[0]
for (var i = 0; i < NUMAGENT; i++) {
  Atomics.store(ia, SPIN + i, 1);
  $262.agent.sleep(500);
}

// Wake them up one at a time and check the order is 0 1 2
for (var i = 0; i < NUMAGENT; i++) {
  assert.sameValue(Atomics.wake(ia, WAKEUP, 1), 1);
  assert.sameValue(getReport(), i + "ok");
}

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null)
    $262.agent.sleep(100);
  return r;
}

function waitUntil(ia, k, value) {
  var i = 0;
  while (Atomics.load(ia, k) !== value && i < 15) {
    $262.agent.sleep(100);
    i++;
  }
  assert.sameValue(Atomics.load(ia, k), value, "All agents are running");
}
